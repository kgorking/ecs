#ifndef ECS_COMPONENT_POOL
#define ECS_COMPONENT_POOL

#include <cstring> // for memcmp
#include <functional>
#include <tuple>
#include <type_traits>
#include <vector>
#include <map>

#include "tls/collect.h"
#include "tls/split.h"
#include "tls/cache.h"

#include "../entity_id.h"
#include "../entity_range.h"
#include "parent_id.h"

#include "component_pool_base.h"
#include "flags.h"
#include "options.h"

// Helpre macro for components that wish to support pmr.
// Declares the 'allocator_type' and default constructor/assignment
#define ECS_USE_PMR(ClassName)                                                                                                             \
	using allocator_type = std::pmr::polymorphic_allocator<>;                                                                              \
                                                                                                                                           \
	ClassName() : ClassName(allocator_type{}) {}                                                                                           \
	ClassName(ClassName const&) = default;                                                                                                 \
	ClassName(ClassName&&) = default;                                                                                                      \
	~ClassName() = default;                                                                                                                \
                                                                                                                                           \
	ClassName& operator=(ClassName const&) = default;                                                                                      \
	ClassName& operator=(ClassName&&) = default


namespace ecs::detail {

template <class ForwardIt, class BinaryPredicate>
ForwardIt std_combine_erase(ForwardIt first, ForwardIt last, BinaryPredicate p) {
	if (first == last)
		return last;

	ForwardIt result = first;
	while (++first != last) {
		auto const pred_res = p(*result, *first);
		if (!pred_res && ++result != first) {
			*result = std::move(*first);
		}
	}
	return ++result;
}

template <class Cont, class BinaryPredicate>
void combine_erase(Cont& cont, BinaryPredicate p) {
	auto const end = std_combine_erase(cont.begin(), cont.end(), p);
	cont.erase(end, cont.end());
}

template <typename T>
class component_pool final : public component_pool_base {
private:
	static_assert(!is_parent<T>::value, "can not have pools of any ecs::parent<type>");

	using clock = std::chrono::steady_clock;
	using time_point = std::chrono::time_point<clock>;

	struct chunk {
		// The full range this chunk covers.
		entity_range range;

		// The partial range of active entities inside this chunk
		entity_range active;

		// The data for the full range of the chunk (range.count())
		T* data = nullptr;

		// time_point last_modified;

		//
		chunk* next = nullptr;

		// True if this chunk is responsible for freeing up the data
		// when it is no longer in use.
		bool owns_data : 1 = false;

		// True if this chunk has been split.
		bool split_data : 1 = false;
	};

	std::allocator<T> alloc;

	chunk* head = nullptr;

	// Cache of all active ranges
	std::vector<entity_range> cached_ranges;

	// Keep track of which components to add/remove each cycle
	using entity_data = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, T>>;
	using entity_span = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, std::span<const T>>>;
	tls::collect<std::vector<entity_data>, component_pool<T>> deferred_adds;
	tls::collect<std::vector<entity_span>, component_pool<T>> deferred_spans;
	tls::collect<std::vector<entity_range>, component_pool<T>> deferred_removes;

	std::map<entity_range, chunk*> range_to_chunk_map;
	mutable tls::split<chunk*, component_pool<T>> chunk_caches;

	// Status flags
	bool components_added = false;
	bool components_removed = false;
	bool components_modified = false;

public:
	~component_pool() override {
		free_all_chunks();
	}

	// Add a span of component to a range of entities
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	constexpr void add_span(entity_range const range, std::span<const T> span) requires(!detail::unbound<T>) {
		Expects(range.count() == std::ssize(span));

		// Add the range and function to a temp storage
		deferred_spans.local().emplace_back(range, span);
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	void add(entity_range const range, T&& component) {
		if constexpr (tagged<T>) {
			deferred_adds.local().push_back(range);
		} else {
			deferred_adds.local().emplace_back(range, std::forward<T>(component));
		}
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	void add(entity_range const range, T const& component) {
		if constexpr (tagged<T>) {
			deferred_adds.local().push_back(range);
		} else {
			deferred_adds.local().emplace_back(range, component);
		}
	}

	// Return the shared component
	T& get_shared_component() requires unbound<T> {
		static T t{};
		return t;
	}

	// Remove an entity from the component pool. This logically removes the component from the
	// entity.
	void remove(entity_id const id) {
		remove({id, id});
	}

	// Remove an entity from the component pool. This logically removes the component from the
	// entity.
	void remove(entity_range const range) {
		deferred_removes.local().push_back(range);
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	T* find_component_data(entity_id const id) {
		return const_cast<T*>(std::as_const(*this).find_component_data(id));
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	T const* find_component_data(entity_id const id) const {
		if (head == nullptr)
			return nullptr;
		
		auto const it = range_to_chunk_map.lower_bound({id, id});
		if (it != range_to_chunk_map.end() && it->second->active.contains(id)) {
			auto const offset = it->second->range.offset(id);
			return &it->second->data[offset];
		}

		return nullptr;
	}

	// Merge all the components queued for addition to the main storage,
	// and remove components queued for removal
	void process_changes() override {
		process_remove_components();
		process_add_components();

		// TODO? collapse_adjacent_ranges()

		// Sort it
		// New ranges are never added to l1, but existing l1
		// can be downgraded to l2.
		/*if (l0_size_start == 0 && !l0.empty()) {
			std::ranges::sort(l0, std::less{}, &level_0::range);
		} else if (l0_size_start != l0.size()) {
			auto const middle = l0.begin() + l0_size_start;

			// sort the newcomers
			std::ranges::sort(middle, l0.end(), std::less{}, &level_0::range);

			// merge them into the rest
			std::ranges::inplace_merge(l0, middle, std::less{}, &level_0::range);
		}
		if (l2_size_start == 0 && !l2.empty()) {
			std::ranges::sort(l2, std::less{}, &level_2::range);
		} else if (l2_size_start != l2.size()) {
			auto const middle = l2.begin() + l2_size_start;

			// sort the newcomers
			std::ranges::sort(middle, l2.end(), std::less{}, &level_2::range);

			// merge them into the rest
			std::ranges::inplace_merge(l2, middle, std::less{}, &level_2::range);
		}*/

		update_cached_ranges();
	}

	// Returns the number of active entities in the pool
	size_t num_entities() const {
		size_t count = 0;

		auto curr = head;
		while (nullptr != curr) {
			count += curr->active.ucount();
			curr = curr->next;
		}

		return count;
	}

	// Returns the number of active components in the pool
	size_t num_components() const {
		if constexpr (unbound<T>)
			return 1;
		else
			return num_entities();
	}

	// Returns the number of chunks in use
	size_t num_chunks() const noexcept {
		size_t count = 0;

		auto curr = head;
		while (nullptr != curr) {
			count += 1;
			curr = curr->next;
		}

		return count;
	}

	chunk const* const get_head_chunk() const noexcept {
		return head;
	}

	// Clears the pools state flags
	void clear_flags() override {
		components_added = false;
		components_removed = false;
		components_modified = false;
	}

	// Returns true if components has been added since last clear_flags() call
	bool has_more_components() const {
		return components_added;
	}

	// Returns true if components has been removed since last clear_flags() call
	bool has_less_components() const {
		return components_removed;
	}

	// Returns true if components has been added/removed since last clear_flags() call
	bool has_component_count_changed() const {
		return components_added || components_removed;
	}

	bool has_components_been_modified() const {
		return has_component_count_changed() || components_modified;
	}

	// Returns the pools entities
	entity_range_view get_entities() const {
		if constexpr (detail::global<T>) {
			// globals are accessible to all entities
			static constexpr entity_range global_range{std::numeric_limits<ecs::detail::entity_type>::min(),
													   std::numeric_limits<ecs::detail::entity_type>::max()};
			return entity_range_view{&global_range, 1};
		} else {
			return cached_ranges;
		}
	}

	// Returns true if an entity has a component in this pool
	bool has_entity(entity_id const id) const {
		return has_entity({id, id});
	}

	// Returns true if an entity range has components in this pool
	bool has_entity(entity_range const& range) const {
		auto curr = head;
		while (nullptr != curr) {
			if (curr->active.contains(range))
				return true;
			curr = curr->next;
		}
		return false;
	}

	// TODO remove?
	// Checks the current threads queue for the entity
	bool is_queued_add(entity_id const id) {
		return is_queued_add({id, id});
	}

	// Checks the current threads queue for the entity
	bool is_queued_add(entity_range const& range) {
		if (deferred_adds.local().empty()) {
			return false;
		}

		for (auto const& ents : deferred_adds.local()) {
			if (std::get<0>(ents).contains(range)) {
				return true;
			}
		}

		return false;
	}

	// Checks the current threads queue for the entity
	bool is_queued_remove(entity_id const id) {
		return is_queued_remove({id, id});
	}

	// Checks the current threads queue for the entity
	bool is_queued_remove(entity_range const& range) {
		if (deferred_removes.local().empty())
			return false;

		for (auto const& ents : deferred_removes.local()) {
			if (ents.contains(range))
				return true;
		}

		return false;
	}

	// Clear all entities from the pool
	void clear() override {
		// Remember if components was removed from the pool
		bool const is_removed = (nullptr != head);

		// Clear the pool
		chunk* curr = head;
		while (curr != nullptr) {
			chunk* next = curr->next;
			if (curr->owns_data) {
				alloc.deallocate(curr->data, curr->range.count());
			}

			delete curr;
			curr = next;
		}
		head = nullptr;

		deferred_adds.reset();
		deferred_spans.reset();
		deferred_removes.reset();
		clear_flags();

		// Save the removal state
		components_removed = is_removed;
	}

	// Flag that components has been modified
	void notify_components_modified() {
		components_modified = true;
	}

private:
	chunk* create_new_chunk(std::forward_iterator auto iter) {
		entity_range const r = std::get<0>(*iter);
		chunk* c = new chunk{r, r, nullptr, nullptr, true};
		if constexpr (!unbound<T>) {
			c->data = alloc.allocate(r.ucount());
			construct_range_in_chunk(c, r, std::get<1>(*iter));
		}

		range_to_chunk_map[r] = c;

		return c;
	}

	void free_chunk(chunk* c) {
		range_to_chunk_map.erase(c->range);

		if (c->owns_data) {
			if (c->split_data && nullptr != c->next) {
				// transfer ownership
				c->next->owns_data = true;
			} else {
				std::destroy_n(c->data, c->active.ucount());
				alloc.deallocate(c->data, c->range.count());
			}
		}

		delete c;
	}

	void free_all_chunks() {
		chunk* curr = head;
		while (curr != nullptr) {
			chunk* next = curr->next;
			free_chunk(curr);
			curr = next;
		}
		head = nullptr;
		set_data_removed();
	}

	// Flag that components has been added
	void set_data_added() {
		components_added = true;
	}

	// Flag that components has been removed
	void set_data_removed() {
		components_removed = true;
	}

	void update_cached_ranges() {
		if (!components_added && !components_removed)
			return;

		// Find all ranges
		cached_ranges.clear();
		chunk* curr = head;
		while (curr != nullptr) {
			cached_ranges.push_back(curr->active);
			curr = curr->next;
		}
	}

	// Verify the 'add*' functions precondition.
	// An entity can not have more than one of the same component
	bool has_duplicate_entities() const {
		chunk* curr = head;
		while (curr != nullptr && curr->next != nullptr) {
			if (curr->active.overlaps(curr->next->active))
				return true;
			curr = curr->next;
		}
		return false;
	};

	constexpr static bool is_equal(T const& lhs, T const& rhs) requires std::equality_comparable<T> {
		return lhs == rhs;
	}
	constexpr static bool is_equal(T const& /*lhs*/, T const& /*rhs*/) requires tagged<T> {
		// Tags are empty, so always return true
		return true;
	}
	constexpr static bool is_equal(T const&, T const&) {
		// Type can not be compared, so always return false.
		// memcmp is a no-go because it also compares padding in types,
		// and it is not constexpr
		return false;
	}

	template<typename Data>
	void construct_range_in_chunk(chunk* c, entity_range range, Data const& comp_data) {
		if constexpr (!unbound<T>) {
			// Offset into the chunks data
			auto const ent_offset = c->range.offset(range.first());

			for (entity_offset i = 0; i < range.ucount(); ++i) {
				// Construct from a value or a a span of values
				if constexpr (std::is_same_v<T, Data>) {
					std::construct_at(&c->data[ent_offset + i], comp_data);
				} else {
					std::construct_at(&c->data[ent_offset + i], comp_data[i]);
				}
			}
		}
	}

	void fill_data_in_existing_chunk(chunk*& curr, chunk*& prev, entity_range r) {
		// If split chunks are encountered, skip forward to the chunk closest to r
		if (curr->split_data) {
			while (nullptr != curr->next && curr->next->range.contains(r) && curr->next->active < r) {
				prev = curr;
				curr = curr->next;
			}
		}

		if (curr->active.adjacent(r)) {
			// The two ranges are next to each other, so add the data to existing chunk
			curr->active = entity_range::merge(curr->active, r);

			chunk* next = curr->next;

			// Check to see if this chunk can be collapsed into 'prev'
			if (nullptr != prev) {
				if (curr->active.adjacent(prev->active)) {
					curr->active = entity_range::merge(curr->active, prev->active);
					free_chunk(curr);
					prev->next = next;
					curr = next;
					if (next != nullptr)
						next = next->next;
				}
			}

			// Check to see if 'next' can be collapsed into this chunk
			if (nullptr != next) {
				if (curr->active.adjacent(next->active)) {
					curr->active = entity_range::merge(curr->active, next->active);
					curr->next = next->next;

					// split_data is true if the next chunk is also in the current range
					curr->split_data = (curr->next != nullptr) && (curr->range == curr->next->range);

					free_chunk(next);
				}
			}
		} else {
			// There is a gap between the two ranges, so split the chunk
			if (r < curr->active) {
				bool const is_head_chunk = (head == curr);
				bool const curr_owns_data = curr->owns_data;
				curr->owns_data = false;
				curr = new chunk{curr->range, r, curr->data, curr, curr_owns_data, true};

				// Update head pointer
				if (is_head_chunk)
					head = curr;

				// Make the previous chunk point to curr
				if (prev != nullptr)
					prev->next = curr;
			} else {
				curr->split_data = true;
				curr->next = new chunk{curr->range, r, curr->data, curr->next, false, false};
			}
		}
	}

	// Add new queued entities and components to the main storage.
	void process_add_components() {
		// Combine the components in to a single vector
		std::vector<entity_data> adds;
		std::vector<entity_span> spans;
		deferred_adds.gather_flattened(std::back_inserter(adds));
		deferred_spans.gather_flattened(std::back_inserter(spans));

		if (adds.empty() && spans.empty()) {
			return;
		}

		// Clear the current adds
		deferred_adds.reset();
		deferred_spans.reset();

		// Sort the input
		auto const comparator = [](auto const& l, auto const& r) {
			return std::get<0>(l).first() < std::get<0>(r).first();
		};
		std::sort(adds.begin(), adds.end(), comparator);
		std::sort(spans.begin(), spans.end(), comparator);


		// Merge adjacent ranges that has the same data
		if constexpr (!detail::unbound<T>) { // contains data
			auto const combiner = [](entity_data& a, entity_data const& b) {
				auto& [a_rng, a_data] = a;
				auto const& [b_rng, b_data] = b;

				if (a_rng.adjacent(b_rng) && is_equal(a_data, b_data)) {
					a_rng = entity_range::merge(a_rng, b_rng);
					return true;
				} else {
					return false;
				}
			};
			combine_erase(adds, combiner);
		} else {											   // does not contain data
			auto const combiner = [](auto& a, auto const& b) { // entity_data/entity_init
				auto& [a_rng] = a;
				auto const& [b_rng] = b;

				if (a_rng.adjacent(b_rng)) {
					a_rng = entity_range::merge(a_rng, b_rng);
					return true;
				} else {
					return false;
				}
			};
			combine_erase(adds, combiner);
		}

		// Do the insertions
		chunk* prev = nullptr;
		chunk* curr = head;

		auto it_adds = adds.begin();
		auto it_spans = spans.begin();

		// Create head chunk if needed
		if (head == nullptr) {
			if (it_adds != adds.end()) {
				head = create_new_chunk(it_adds);
				++it_adds;
			} else {
				head = create_new_chunk(it_spans);
				++it_spans;
			}

			curr = head;
		}

		auto const merge_data = [&](std::forward_iterator auto& iter) {
			if (curr == nullptr) {
				auto new_chunk = create_new_chunk(iter);
				new_chunk->next = curr;
				curr = new_chunk;
				prev->next = curr;
				++iter;
			} else {
				entity_range const r = std::get<0>(*iter);

				// Move current chunk pointer forward
				while (nullptr != curr->next && curr->next->range.contains(r) && curr->next->active < r) {
					prev = curr;
					curr = curr->next;
				}

				if (curr->range.overlaps(r)) {
					// Incoming range overlaps the current one, so add it into 'curr'
					fill_data_in_existing_chunk(curr, prev, r);
					if constexpr (!unbound<T>) {
						construct_range_in_chunk(curr, r, std::get<1>(*iter));
					}
					++iter;
				} else if (curr->range < r) {
					// Incoming range is larger than the current one, so add it after 'curr'
					auto new_chunk = create_new_chunk(iter);
					new_chunk->next = curr->next;
					curr->next = new_chunk;

					prev = curr;
					curr = curr->next;

					++iter;
				} else if (r < curr->range) {
					// Incoming range is less than the current one, so add it before 'curr' (after 'prev')
					auto new_chunk = create_new_chunk(iter);
					new_chunk->next = curr;
					if (head == curr)
						head = new_chunk;
					curr = new_chunk;
					if (prev != nullptr)
						prev->next = curr;
					++iter;
				}
			}
		};

		// Fill in values
		while (it_adds != adds.end()) {
			merge_data(it_adds);
		}

		// Fill in spans
		prev = nullptr;
		curr = head;
		while (it_spans != spans.end()) {
			merge_data(it_spans);
		}

		// Check it
		Expects(false == has_duplicate_entities());

		// Update the state
		set_data_added();
	}

	// Removes the entities and components
	void process_remove_components() {
		// Collect all the ranges to remove
		std::vector<entity_range> vec;
		deferred_removes.gather_flattened(std::back_inserter(vec));

		// Dip if there is nothing to do
		if (vec.empty()) {
			return;
		}

		// Sort the ranges to remove
		std::ranges::sort(vec, std::ranges::less{}, &entity_range::first);

		// Remove level 0 ranges, or downgrade them to level_ 1 or 2
		if (nullptr != head) {
			process_remove_components_level_0(vec);
		}

		// Update the state
		set_data_removed();
	}

	void process_remove_components_level_0(std::vector<entity_range>& removes) {
		chunk* prev = nullptr; 
		chunk* it_l0 = head;
		auto it_rem = removes.begin();

		while (it_l0 != nullptr && it_rem != removes.end()) {
			if (it_l0->active < *it_rem) {
				prev = it_l0;
				it_l0 = it_l0->next;
			} else if (*it_rem < it_l0->active) {
				++it_rem;
			}
			else {
				if (it_l0->active == *it_rem) {
					// remove an entire range
					// todo: move to a free-store?
					chunk* next = it_l0->next;

					// Update head pointer
					if (it_l0 == head) {
						head = next;
					}

					// Delete the chunk and potentially its data
					free_chunk(it_l0);

					// Update the previous chunks next pointer
					if (nullptr != prev)
						prev->next = next;

					it_l0 = next;
				} else {
					// remove partial range
					auto const [left_range, maybe_split_range] = entity_range::remove(it_l0->active, *it_rem);
					
					// Destroy the removed components
					auto const offset = it_l0->range.offset(it_rem->first());
					std::destroy_n(&it_l0->data[offset], it_rem->ucount());

					if (maybe_split_range.has_value()) {
						// If two ranges were returned, split this chunk
						it_l0->active = left_range;
						it_l0->split_data = true;
						it_l0->next = new chunk{it_l0->range, maybe_split_range.value(), it_l0->data, it_l0->next, false};
					} else {
						// shrink the active range
						it_l0->active = left_range;
					}

					prev = it_l0;
					it_l0 = it_l0->next;
				}
			}
		}
	}

	// Removes transient components
	void process_remove_components() requires transient<T> {
		// All transient components are removed each cycle
		free_all_chunks();
	}
};
} // namespace ecs::detail

#endif // !ECS_COMPONENT_POOL

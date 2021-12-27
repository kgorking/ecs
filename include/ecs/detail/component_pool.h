#ifndef ECS_COMPONENT_POOL
#define ECS_COMPONENT_POOL

#include <cstring> // for memcmp
#include <functional>
#include <tuple>
#include <type_traits>
#include <vector>
#include <map>
#include <ranges>

#include "tls/collect.h"

#include "../entity_id.h"
#include "../entity_range.h"
#include "parent_id.h"

#include "component_pool_base.h"
#include "flags.h"
#include "options.h"

namespace ecs::detail {

template <class ForwardIt, class BinaryPredicate>
ForwardIt std_combine_erase(ForwardIt first, ForwardIt last, BinaryPredicate&& p) noexcept {
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
void combine_erase(Cont& cont, BinaryPredicate&& p) noexcept {
	auto const end = std_combine_erase(cont.begin(), cont.end(), std::forward<BinaryPredicate>(p));
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

	// Keep track of which components to add/remove each cycle
	using entity_data = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, T>>;
	using entity_span = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, std::span<const T>>>;
	tls::collect<std::vector<entity_data>, component_pool<T>> deferred_adds;
	tls::collect<std::vector<entity_span>, component_pool<T>> deferred_spans;
	tls::collect<std::vector<entity_range>, component_pool<T>> deferred_removes;

	std::vector<entity_range> ranges;
	std::vector<chunk*> chunks;

	// Status flags
	bool components_added = false;
	bool components_removed = false;
	bool components_modified = false;

public:
	component_pool() noexcept = default;
	component_pool(component_pool const&) = delete;
	component_pool(component_pool &&) = delete;
	component_pool& operator=(component_pool const&) = delete;
	component_pool& operator=(component_pool&&) = delete;
	~component_pool() noexcept override {
		free_all_chunks();
	}

	// Add a span of component to a range of entities
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	void add_span(entity_range const range, std::span<const T> span) noexcept requires(!detail::unbound<T>) {
		Expects(range.count() == std::ssize(span));

		// Add the range and function to a temp storage
		deferred_spans.local().emplace_back(range, span);
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	void add(entity_range const range, T&& component) noexcept {
		if constexpr (tagged<T>) {
			deferred_adds.local().push_back(range);
		} else {
			deferred_adds.local().emplace_back(range, std::forward<T>(component));
		}
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	void add(entity_range const range, T const& component) noexcept {
		if constexpr (tagged<T>) {
			deferred_adds.local().push_back(range);
		} else {
			deferred_adds.local().emplace_back(range, component);
		}
	}

	// Return the shared component
	T& get_shared_component() noexcept requires unbound<T> {
		static T t{};
		return t;
	}

	// Remove an entity from the component pool. This logically removes the component from the
	// entity.
	void remove(entity_id const id) noexcept {
		remove({id, id});
	}

	// Remove an entity from the component pool. This logically removes the component from the
	// entity.
	void remove(entity_range const range) noexcept {
		deferred_removes.local().push_back(range);
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	T* find_component_data(entity_id const id) noexcept {
		return const_cast<T*>(std::as_const(*this).find_component_data(id));
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	T const* find_component_data(entity_id const id) const noexcept {
		if (head == nullptr)
			return nullptr;
		
		auto const range_it = find_in_ranges_vec({id, id});
		if (range_it != ranges.end()) {
			auto const chunk_it = chunks.begin() + ranges_dist(range_it);
			chunk* c = (*chunk_it);
			if (c->active.contains(id)) {
				auto const offset = c->range.offset(id);
				return &c->data[offset];
			}
		}

		return nullptr;
	}

	// Merge all the components queued for addition to the main storage,
	// and remove components queued for removal
	void process_changes() noexcept override {
		process_remove_components();
		process_add_components();

		// TODO? collapse_adjacent_ranges()
	}

	// Returns the number of active entities in the pool
	size_t num_entities() const noexcept {
		size_t count = 0;

		for (entity_range const r : ranges) {
			count += r.ucount();
		}

		return count;
	}

	// Returns the number of active components in the pool
	size_t num_components() const noexcept {
		if constexpr (unbound<T>)
			return 1;
		else
			return num_entities();
	}

	// Returns the number of chunks in use
	size_t num_chunks() const noexcept {
		return chunks.size();
	}

	chunk const* get_head_chunk() const noexcept {
		return head;
	}

	// Clears the pools state flags
	void clear_flags() noexcept override {
		components_added = false;
		components_removed = false;
		components_modified = false;
	}

	// Returns true if components has been added since last clear_flags() call
	bool has_more_components() const noexcept {
		return components_added;
	}

	// Returns true if components has been removed since last clear_flags() call
	bool has_less_components() const noexcept {
		return components_removed;
	}

	// Returns true if components has been added/removed since last clear_flags() call
	bool has_component_count_changed() const noexcept {
		return components_added || components_removed;
	}

	bool has_components_been_modified() const noexcept {
		return has_component_count_changed() || components_modified;
	}

	// Returns the pools entities
	entity_range_view get_entities() const noexcept {
		if constexpr (detail::global<T>) {
			// globals are accessible to all entities
			static constinit entity_range global_range = entity_range::all();
			return entity_range_view{&global_range, 1};
		} else {
			return ranges;
		}
	}

	// Returns true if an entity has a component in this pool
	bool has_entity(entity_id const id) const noexcept {
		return has_entity({id, id});
	}

	// Returns true if an entity range has components in this pool
	bool has_entity(entity_range const& range) const noexcept {
		auto curr = head;
		while (nullptr != curr) {
			if (curr->active.contains(range))
				return true;
			curr = curr->next;
		}
		return false;
	}

	// Clear all entities from the pool
	void clear() noexcept override {
		// Remember if components was removed from the pool
		bool const is_removed = (nullptr != head);

		// Clear all data
		free_all_chunks();
		deferred_adds.reset();
		deferred_spans.reset();
		deferred_removes.reset();
		ranges.clear();
		chunks.clear();
		clear_flags();

		// Save the removal state
		components_removed = is_removed;
	}

	// Flag that components has been modified
	void notify_components_modified() noexcept {
		components_modified = true;
	}

private:
	chunk* create_new_chunk(entity_range const range, entity_range const active, T* data = nullptr, chunk* next = nullptr,
							bool owns_data = true, bool split_data = false) noexcept {
		chunk* c = new chunk{range, active, data, next, owns_data, split_data};
		auto const range_it = find_in_ranges_vec(active);
		auto const dist = ranges_dist(range_it);
		ranges.insert(range_it, active);

		auto const chunk_it = chunks.begin() + dist;
		chunks.insert(chunk_it, c);

		return c;
	}

	chunk* create_new_chunk(std::forward_iterator auto iter) noexcept {
		entity_range const r = std::get<0>(*iter);
		chunk* c = create_new_chunk(r, r);
		if constexpr (!unbound<T>) {
			c->data = alloc.allocate(r.ucount());
			construct_range_in_chunk(c, r, std::get<1>(*iter));
		}

		return c;
	}

	void free_chunk(chunk* c) noexcept {
		remove_range_to_chunk(c->active);
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

	void free_all_chunks() noexcept {
		ranges.clear();
		chunks.clear();
		chunk* curr = head;
		while (curr != nullptr) {
			chunk* next = curr->next;
			free_chunk(curr);
			curr = next;
		}
		head = nullptr;
		set_data_removed();
	}

	auto find_in_ranges_vec(entity_range const rng) noexcept {
		return std::ranges::lower_bound(ranges, rng, std::less{});
	}
	auto find_in_ranges_vec(entity_range const rng) const noexcept {
		return std::ranges::lower_bound(ranges, rng, std::less{});
	}

	ptrdiff_t ranges_dist(std::vector<entity_range>::const_iterator it) const noexcept {
		return std::distance(ranges.begin(), it);
	}

	// Removes a range and chunk from the map
	void remove_range_to_chunk(entity_range const rng) noexcept {
		auto const it = find_in_ranges_vec(rng);
		if (it != ranges.end() && *it == rng) {
			auto const dist = ranges_dist(it);

			ranges.erase(it);
			chunks.erase(chunks.begin() + dist);
		}
	}
	
	// Updates a key in the range-to-chunk map
	void update_range_to_chunk_key(entity_range const old, entity_range const update) noexcept {
		auto it = find_in_ranges_vec(old);
		*it = update;
	}

	// Flag that components has been added
	void set_data_added() noexcept {
		components_added = true;
	}

	// Flag that components has been removed
	void set_data_removed() noexcept {
		components_removed = true;
	}

	// Verify the 'add*' functions precondition.
	// An entity can not have more than one of the same component
	bool has_duplicate_entities() const noexcept {
		chunk* curr = head;
		while (curr != nullptr && curr->next != nullptr) {
			if (curr->active.overlaps(curr->next->active))
				return true;
			curr = curr->next;
		}
		return false;
	};

	constexpr static bool is_equal(T const& lhs, T const& rhs) noexcept requires std::equality_comparable<T> {
		return lhs == rhs;
	}
	constexpr static bool is_equal(T const& /*lhs*/, T const& /*rhs*/) noexcept requires tagged<T> {
		// Tags are empty, so always return true
		return true;
	}
	constexpr static bool is_equal(T const&, T const&) noexcept {
		// Type can not be compared, so always return false.
		// memcmp is a no-go because it also compares padding in types,
		// and it is not constexpr
		return false;
	}

	template<typename Data>
	void construct_range_in_chunk(chunk* c, entity_range range, Data const& comp_data) noexcept {
		Expects(c != nullptr);
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

	void fill_data_in_existing_chunk(chunk*& curr, chunk*& prev, entity_range r) noexcept {
		// If split chunks are encountered, skip forward to the chunk closest to r
		if (curr->split_data) {
			while (nullptr != curr->next && curr->next->range.contains(r) && curr->next->active < r) {
				prev = curr;
				curr = curr->next;
			}
		}

		if (curr->active.adjacent(r)) {
			// The two ranges are next to each other, so add the data to existing chunk
			entity_range active_range = entity_range::merge(curr->active, r);
			update_range_to_chunk_key(curr->active, active_range);
			curr->active = active_range;

			chunk* next = curr->next;

			// Check to see if this chunk can be collapsed into 'prev'
			if (nullptr != prev) {
				if (prev->active.adjacent(curr->active)) {
					active_range = entity_range::merge(prev->active, curr->active);
					remove_range_to_chunk(prev->active);
					update_range_to_chunk_key(prev->active, active_range);
					prev->active = active_range;

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
					active_range = entity_range::merge(curr->active, next->active);
					remove_range_to_chunk(next->active);
					update_range_to_chunk_key(curr->active, active_range);

					curr->active = active_range;
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
 				curr = create_new_chunk(curr->range, r, curr->data, curr, curr_owns_data, true);

				// Update head pointer
				if (is_head_chunk)
					head = curr;

				// Make the previous chunk point to curr
				if (prev != nullptr)
					prev->next = curr;
			} else {
				curr->split_data = true;
				curr->next = create_new_chunk(curr->range, r, curr->data, curr->next, false, false);
			}
		}
	}

	// Try to combine two ranges. With data
	constexpr static bool combiner_bound(entity_data& a, entity_data const& b) requires(!unbound<T>) {
		auto& [a_rng, a_data] = a;
		auto const& [b_rng, b_data] = b;

		if (a_rng.adjacent(b_rng) && is_equal(a_data, b_data)) {
			a_rng = entity_range::merge(a_rng, b_rng);
			return true;
		} else {
			return false;
		}
	}

	// Try to combine two ranges. Without data
	constexpr static bool combiner_unbound(entity_data& a, entity_data const& b) requires(unbound<T>) {
		auto& [a_rng] = a;
		auto const& [b_rng] = b;

		if (a_rng.adjacent(b_rng)) {
			a_rng = entity_range::merge(a_rng, b_rng);
			return true;
		} else {
			return false;
		}
	}


	// Add new queued entities and components to the main storage.
	void process_add_components() noexcept {
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
		if constexpr (unbound<T>)
			combine_erase(adds, combiner_unbound);
		else
			combine_erase(adds, combiner_bound);

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
				} else if (curr->range < r) {
					// Incoming range is larger than the current one, so add it after 'curr'
					auto new_chunk = create_new_chunk(iter);
					new_chunk->next = curr->next;
					curr->next = new_chunk;

					prev = curr;
					curr = curr->next;

				} else if (r < curr->range) {
					// Incoming range is less than the current one, so add it before 'curr' (after 'prev')
					auto new_chunk = create_new_chunk(iter);
					new_chunk->next = curr;
					if (head == curr)
						head = new_chunk;
					curr = new_chunk;
					if (prev != nullptr)
						prev->next = curr;
				}
			}
		};

		// Fill in values
		while (it_adds != adds.end()) {
			merge_data(it_adds);
			++it_adds;
		}

		// Fill in spans
		prev = nullptr;
		curr = head;
		while (it_spans != spans.end()) {
			merge_data(it_spans);
			++it_spans;
		}

		// Check it
		Expects(false == has_duplicate_entities());

		// Update the state
		set_data_added();
	}

	// Removes the entities and components
	void process_remove_components() noexcept {
		// Collect all the ranges to remove
		std::vector<entity_range> vec;
		deferred_removes.gather_flattened(std::back_inserter(vec));

		// Dip if there is nothing to do
		if (vec.empty()) {
			return;
		}

		// Sort the ranges to remove
		std::ranges::sort(vec, std::ranges::less{}, &entity_range::first);

		// Remove ranges
		if (nullptr != head) {
			process_remove_components(vec);
		}

		// Update the state
		set_data_removed();
	}

	void process_remove_components(std::vector<entity_range>& removes) noexcept {
		chunk* prev = nullptr; 
		chunk* it_chunk = head;
		auto it_rem = removes.begin();

		while (it_chunk != nullptr && it_rem != removes.end()) {
			if (it_chunk->active < *it_rem) {
				prev = it_chunk;
				it_chunk = it_chunk->next;
			} else if (*it_rem < it_chunk->active) {
				++it_rem;
			}
			else {
				if (it_chunk->active == *it_rem) {
					// remove an entire range
					// todo: move to a free-store?
					chunk* next = it_chunk->next;

					// Update head pointer
					if (it_chunk == head) {
						head = next;
					}

					// Delete the chunk and potentially its data
					free_chunk(it_chunk);

					// Update the previous chunks next pointer
					if (nullptr != prev)
						prev->next = next;

					it_chunk = next;
				} else {
					// remove partial range
					auto const [left_range, maybe_split_range] = entity_range::remove(it_chunk->active, *it_rem);

					// Update the active range
					update_range_to_chunk_key(it_chunk->active, left_range);
					it_chunk->active = left_range;

					// Destroy the removed components
					auto const offset = it_chunk->range.offset(it_rem->first());
					std::destroy_n(&it_chunk->data[offset], it_rem->ucount());

					if (maybe_split_range.has_value()) {
						// If two ranges were returned, split this chunk
						it_chunk->split_data = true;
						it_chunk->next = create_new_chunk(it_chunk->range, maybe_split_range.value(), it_chunk->data, it_chunk->next, false);
					}

					prev = it_chunk;
					it_chunk = it_chunk->next;
				}
			}
		}
	}

	// Removes transient components
	void process_remove_components() noexcept requires transient<T> {
		// All transient components are removed each cycle
		free_all_chunks();
	}
};
} // namespace ecs::detail

#endif // !ECS_COMPONENT_POOL

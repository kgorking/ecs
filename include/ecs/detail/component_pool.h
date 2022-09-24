#ifndef ECS_DETAIL_COMPONENT_POOL_H
#define ECS_DETAIL_COMPONENT_POOL_H

#include <execution>
#include <functional>
#include <memory>
#include <vector>

#include "tls/collect.h"

#include "../entity_id.h"
#include "../entity_range.h"
#include "parent_id.h"

#include "component_pool_base.h"
#include "flags.h"
#include "options.h"

namespace ecs::detail {

constexpr static std::size_t parallelization_size_tipping_point = 4096;

template <class ForwardIt, class BinaryPredicate>
constexpr ForwardIt std_combine_erase(ForwardIt first, ForwardIt last, BinaryPredicate&& p) noexcept {
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
constexpr void combine_erase(Cont& cont, BinaryPredicate&& p) noexcept {
	auto const end = std_combine_erase(cont.begin(), cont.end(), static_cast<BinaryPredicate&&>(p));
	cont.erase(end, cont.end());
}

template <typename T, typename Alloc = std::allocator<T>>
class component_pool final : public component_pool_base {
private:
	static_assert(!is_parent<T>::value, "can not have pools of any ecs::parent<type>");

	using allocator_type = Alloc;

	struct chunk {
		constexpr chunk(entity_range range_, entity_range active_, T* data_ = nullptr, chunk* next_ = nullptr, bool owns_data_ = false,
						bool has_split_data_ = false) noexcept
			: range(range_), active(active_), data(data_), next(next_), owns_data(owns_data_), has_split_data(has_split_data_) {}

		// The full range this chunk covers.
		entity_range range;

		// The partial range of active entities inside this chunk
		entity_range active;

		// The data for the full range of the chunk (range.count())
		// The tag signals if this chunk owns this data and should clean it up
		T* data;

		// Points to the next chunk in the list.
		// The tag signals if this chunk has been split
		chunk* next;

		bool owns_data;
		bool has_split_data;
	};
	// static_assert(sizeof(chunk) == 32);

	allocator_type alloc;
	std::allocator<chunk> alloc_chunk;

	chunk* head = nullptr;

	//
	struct entity_empty {
		entity_range rng;
		constexpr entity_empty(entity_range r) noexcept : rng{r} {}
	};

	struct entity_data_member : entity_empty {
		T data;
		constexpr entity_data_member(entity_range r, T const& t) noexcept : entity_empty{r}, data(t) {}
		constexpr entity_data_member(entity_range r, T&& t) noexcept : entity_empty{r}, data(std::forward<T>(t)) {}
	};
	struct entity_span_member : entity_empty {
		std::span<const T> data;
		constexpr entity_span_member(entity_range r, std::span<const T> t) noexcept : entity_empty{r}, data(t) {}
	};
	struct entity_gen_member : entity_empty {
		std::function<T(entity_id)> data;
		constexpr entity_gen_member(entity_range r, std::function<T(entity_id)>&& t) noexcept
			: entity_empty{r}, data(std::forward<std::function<T(entity_id)>>(t)) {}
	};

	using entity_data = std::conditional_t<unbound<T>, entity_empty, entity_data_member>;
	using entity_span = std::conditional_t<unbound<T>, entity_empty, entity_span_member>;
	using entity_gen = std::conditional_t<unbound<T>, entity_empty, entity_gen_member>;

	// Keep track of which components to add/remove each cycle
	tls::collect<std::vector<entity_data>, component_pool<T>> deferred_adds;
	tls::collect<std::vector<entity_span>, component_pool<T>> deferred_spans;
	tls::collect<std::vector<entity_gen>, component_pool<T>> deferred_gen;
	tls::collect<std::vector<entity_range>, component_pool<T>> deferred_removes;

	std::vector<entity_range> ordered_active_ranges;
	std::vector<chunk*> ordered_chunks;

	// Status flags
	bool components_added = false;
	bool components_removed = false;
	bool components_modified = false;

public:
	constexpr component_pool() noexcept {
		if constexpr (global<T>) {
			head = alloc_chunk.allocate(1);
			std::construct_at(head, entity_range{0, 0}, entity_range{0, 0}, nullptr, nullptr, true, false);
			head->data = alloc.allocate(1);
			std::construct_at(head->data);
			ordered_active_ranges.push_back(entity_range::all());
			ordered_chunks.push_back(head);
		}
	}
	constexpr component_pool(component_pool const&) = delete;
	constexpr component_pool(component_pool&&) = delete;
	constexpr component_pool& operator=(component_pool const&) = delete;
	constexpr component_pool& operator=(component_pool&&) = delete;
	constexpr ~component_pool() noexcept override {
		if constexpr (global<T>) {
			std::destroy_at(head->data);
			alloc.deallocate(head->data, head->range.ucount());
			std::destroy_at(head);
			alloc_chunk.deallocate(head, 1);
		} else {
			free_all_chunks();
		}
	}

	// Add a span of component to a range of entities
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	constexpr void add_span(entity_range const range, std::span<const T> span) noexcept requires(!detail::unbound<T>) {
		Expects(range.count() == std::ssize(span));

		// Add the range and function to a temp storage
		deferred_spans.local().emplace_back(range, span);
	}

	// Add a component to a range of entities, initialized by the supplied user function generator
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	template <typename Fn>
	void add_generator(entity_range const range, Fn&& gen) {
		// Add the range and function to a temp storage
		deferred_gen.local().emplace_back(range, std::forward<Fn>(gen));
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	constexpr void add(entity_range const range, T&& component) noexcept {
		if constexpr (tagged<T>) {
			deferred_adds.local().emplace_back(range);
		} else {
			deferred_adds.local().emplace_back(range, std::forward<T>(component));
		}
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	constexpr void add(entity_range const range, T const& component) noexcept {
		if constexpr (tagged<T>) {
			deferred_adds.local().emplace_back(range);
		} else {
			deferred_adds.local().emplace_back(range, component);
		}
	}

	// Return the shared component
	constexpr T& get_shared_component() noexcept requires global<T> {
		return head->data[0];
	}

	// Remove an entity from the component pool.
	constexpr void remove(entity_id const id) noexcept {
		remove({id, id});
	}

	// Remove an entity from the component pool.
	constexpr void remove(entity_range const range) noexcept {
		deferred_removes.local().push_back(range);
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	constexpr T* find_component_data(entity_id const id) noexcept requires(!global<T>) {
		return const_cast<T*>(std::as_const(*this).find_component_data(id));
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	constexpr T const* find_component_data(entity_id const id) const noexcept requires(!global<T>) {
		if (head == nullptr)
			return nullptr;

		auto const range_it = find_in_ordered_active_ranges({id, id});
		if (range_it != ordered_active_ranges.end() && range_it->contains(id)) {
			auto const chunk_it = ordered_chunks.begin() + ranges_dist(range_it);
			chunk* c = (*chunk_it);
			auto const offset = c->range.offset(id);
			return &c->data[offset];
		}

		return nullptr;
	}

	// Merge all the components queued for addition to the main storage,
	// and remove components queued for removal
	constexpr void process_changes() noexcept override {
		if constexpr (!global<T>) {
			process_remove_components();
			process_add_components();
		}
	}

	// Returns the number of active entities in the pool
	constexpr ptrdiff_t num_entities() const noexcept {
		ptrdiff_t count = 0;

		for (entity_range const r : ordered_active_ranges) {
			count += r.count();
		}

		return count;
	}

	// Returns the number of active components in the pool
	constexpr ptrdiff_t num_components() const noexcept {
		if constexpr (unbound<T>)
			return 1;
		else
			return num_entities();
	}

	// Returns the number of chunks in use
	constexpr ptrdiff_t num_chunks() const noexcept {
		return std::ssize(ordered_chunks);
	}

	constexpr chunk const* get_head_chunk() const noexcept {
		return head;
	}

	// Clears the pools state flags
	constexpr void clear_flags() noexcept override {
		components_added = false;
		components_removed = false;
		components_modified = false;
	}

	// Returns true if components has been added since last clear_flags() call
	constexpr bool has_more_components() const noexcept {
		return components_added;
	}

	// Returns true if components has been removed since last clear_flags() call
	constexpr bool has_less_components() const noexcept {
		return components_removed;
	}

	// Returns true if components has been added/removed since last clear_flags() call
	constexpr bool has_component_count_changed() const noexcept {
		return components_added || components_removed;
	}

	constexpr bool has_components_been_modified() const noexcept {
		return has_component_count_changed() || components_modified;
	}

	// Returns the pools entities
	constexpr entity_range_view get_entities() const noexcept {
		if constexpr (detail::global<T>) {
			// globals are accessible to all entities
			// static constinit entity_range global_range = entity_range::all();
			// return entity_range_view{&global_range, 1};
			return ordered_active_ranges;
		} else {
			return ordered_active_ranges;
		}
	}

	// Returns true if an entity has a component in this pool
	constexpr bool has_entity(entity_id const id) const noexcept {
		return has_entity({id, id});
	}

	// Returns true if an entity range has components in this pool
	constexpr bool has_entity(entity_range const& range) const noexcept {
		auto const it = find_in_ordered_active_ranges(range);

		if (it == ordered_active_ranges.end())
			return false;

		return it->contains(range);
	}

	// Clear all entities from the pool
	constexpr void clear() noexcept override {
		// Remember if components was removed from the pool
		bool const is_removed = (nullptr != head);

		// Clear all data
		free_all_chunks();
		deferred_adds.reset();
		deferred_spans.reset();
		deferred_gen.reset();
		deferred_removes.reset();
		ordered_active_ranges.clear();
		ordered_chunks.clear();
		clear_flags();

		// Save the removal state
		components_removed = is_removed;
	}

	// Flag that components has been modified
	constexpr void notify_components_modified() noexcept {
		components_modified = true;
	}

private:
	constexpr chunk* create_new_chunk(entity_range const range, entity_range const active, T* data = nullptr, chunk* next = nullptr,
									  bool owns_data = true, bool split_data = false) noexcept {
		chunk* c = alloc_chunk.allocate(1);
		std::construct_at(c, range, active, data, next, owns_data, split_data);

		auto const range_it = find_in_ordered_active_ranges(active);
		auto const dist = ranges_dist(range_it);
		ordered_active_ranges.insert(range_it, active);

		auto const chunk_it = ordered_chunks.begin() + dist;
		ordered_chunks.insert(chunk_it, c);

		return c;
	}

	constexpr chunk* create_new_chunk(std::forward_iterator auto iter) noexcept {
		entity_range const r = iter->rng;
		chunk* c = create_new_chunk(r, r);
		if constexpr (!unbound<T>) {
			c->data = alloc.allocate(r.ucount());
			construct_range_in_chunk(c, r, iter->data);
		}

		return c;
	}

	constexpr void free_chunk(chunk* c) noexcept {
		remove_range_to_chunk(c->active);

		if (c->owns_data) {
			if (c->has_split_data && nullptr != c->next) {
				// transfer ownership
				c->next->owns_data = true;
			} else {
				if constexpr (!unbound<T>) {
					std::destroy_n(c->data, c->active.ucount());
					alloc.deallocate(c->data, c->range.ucount());
				}
			}
		}

		std::destroy_at(c);
		alloc_chunk.deallocate(c, 1);
	}

	constexpr void free_all_chunks() noexcept {
		ordered_active_ranges.clear();
		ordered_chunks.clear();
		chunk* curr = head;
		while (curr != nullptr) {
			chunk* next = curr->next;
			free_chunk(curr);
			curr = next;
		}
		head = nullptr;
		set_data_removed();
	}

	constexpr auto find_in_ordered_active_ranges(entity_range const rng) noexcept {
		return std::ranges::lower_bound(ordered_active_ranges, rng, std::less{});
	}
	constexpr auto find_in_ordered_active_ranges(entity_range const rng) const noexcept {
		return std::ranges::lower_bound(ordered_active_ranges, rng, std::less{});
	}

	constexpr ptrdiff_t ranges_dist(std::vector<entity_range>::const_iterator it) const noexcept {
		return std::distance(ordered_active_ranges.begin(), it);
	}

	// Removes a range and chunk from the map
	constexpr void remove_range_to_chunk(entity_range const rng) noexcept {
		auto const it = find_in_ordered_active_ranges(rng);
		if (it != ordered_active_ranges.end() && *it == rng) {
			auto const dist = ranges_dist(it);

			ordered_active_ranges.erase(it);
			ordered_chunks.erase(ordered_chunks.begin() + dist);
		}
	}

	// Updates a key in the range-to-chunk map
	constexpr void update_range_to_chunk_key(entity_range const old, entity_range const update) noexcept {
		auto it = find_in_ordered_active_ranges(old);
		*it = update;
	}

	// Flag that components has been added
	constexpr void set_data_added() noexcept {
		components_added = true;
	}

	// Flag that components has been removed
	constexpr void set_data_removed() noexcept {
		components_removed = true;
	}

	// Verify the 'add*' functions precondition.
	// An entity can not have more than one of the same component
	constexpr bool has_duplicate_entities() const noexcept {
		for (size_t i = 1; i < ordered_active_ranges.size(); ++i) {
			if (ordered_active_ranges[i - 1].overlaps(ordered_active_ranges[i]))
				return true;
		}

		return false;
	}

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

	template <typename Data>
	constexpr void construct_range_in_chunk(chunk* c, entity_range range, Data const& comp_data) noexcept requires(!unbound<T>) {
		Expects(c != nullptr);

		// Offset into the chunks data
		auto const ent_offset = c->range.offset(range.first());

		entity_id ent = range.first();
		for (size_t i = 0; i < range.ucount(); ++i, ++ent) {
			// Construct from a value or a a span of values
			if constexpr (std::is_same_v<T, Data>) {
				std::construct_at(&c->data[ent_offset + i], comp_data);
			} else if constexpr (std::is_invocable_v<Data, entity_id>) {
				std::construct_at(&c->data[ent_offset + i], comp_data(ent));
			} else {
				std::construct_at(&c->data[ent_offset + i], comp_data[i]);
			}
		}
	}

	constexpr void fill_data_in_existing_chunk(chunk*& curr, chunk*& prev, entity_range r) noexcept {
		// If split chunks are encountered, skip forward to the chunk closest to r
		if (curr->has_split_data) {
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
					curr->has_split_data = (curr->next != nullptr) && (curr->range == curr->next->range);

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
				curr->has_split_data = true;
				curr->next = create_new_chunk(curr->range, r, curr->data, curr->next, false, false);
			}
		}
	}

	// Try to combine two ranges. With data
	constexpr static bool combiner_bound(entity_data& a, entity_data const& b) requires(!unbound<T>) {
		if (a.rng.adjacent(b.rng) && is_equal(a.data, b.data)) {
			a.rng = entity_range::merge(a.rng, b.rng);
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

	template <typename U>
	constexpr void process_add_components(std::vector<U> const& vec) noexcept {
		if (vec.empty()) {
			return;
		}

		// Do the insertions
		chunk* prev = nullptr;
		chunk* curr = head;

		auto it_adds = vec.begin();

		// Create head chunk if needed
		if (head == nullptr) {
			if (it_adds != vec.end()) {
				head = create_new_chunk(it_adds);
				++it_adds;
			}

			curr = head;
		}

		using iterator = typename std::vector<U>::const_iterator;
		auto const merge_data = [&](iterator const& iter) {
			if (curr == nullptr) {
				auto new_chunk = create_new_chunk(iter);
				new_chunk->next = curr;
				curr = new_chunk;
				prev->next = curr;
			} else {
				entity_range const r = iter->rng;

				// Move current chunk pointer forward
				while (nullptr != curr->next && curr->next->range.contains(r) && curr->next->active < r) {
					prev = curr;
					curr = curr->next;
				}

				if (curr->range.overlaps(r)) {
					// Incoming range overlaps the current one, so add it into 'curr'
					fill_data_in_existing_chunk(curr, prev, r);
					if constexpr (!unbound<T>) {
						construct_range_in_chunk(curr, r, iter->data);
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
		while (it_adds != vec.end()) {
			merge_data(it_adds);
			++it_adds;
		}
	}

	// Add new queued entities and components to the main storage.
	constexpr void process_add_components() noexcept {
		auto const adder = [this]<typename C>(std::vector<C>& vec) {
			// Sort the input(s)
			auto const comparator = [](entity_empty const& l, entity_empty const& r) {
				return l.rng < r.rng;
			};
			std::sort(vec.begin(), vec.end(), comparator);

			// Merge adjacent ranges that has the same data
			if constexpr (std::is_same_v<entity_data*, decltype(vec.data())>) {
				if constexpr (unbound<T>)
					combine_erase(vec, combiner_unbound);
				else
					combine_erase(vec, combiner_bound);
			}

			this->process_add_components(vec);
		};

		deferred_adds.for_each(adder);
		deferred_adds.reset();

		deferred_spans.for_each(adder);
		deferred_spans.reset();

		deferred_gen.for_each(adder);
		deferred_gen.reset();

		// Check it
		Expects(false == has_duplicate_entities());

		// Update the state
		set_data_added();
	}

	// Removes the entities and components
	constexpr void process_remove_components() noexcept {
		deferred_removes.for_each([this](std::vector<entity_range>& vec) {
			// Sort the ranges to remove
			std::sort(vec.begin(), vec.end());
			this->process_remove_components(vec);
		});
		deferred_removes.reset();

		// Update the state
		set_data_removed();
	}

	constexpr void process_remove_components(std::vector<entity_range>& removes) noexcept {
		chunk* prev = nullptr;
		chunk* it_chunk = head;
		auto it_rem = removes.begin();

		while (it_chunk != nullptr && it_rem != removes.end()) {
			if (it_chunk->active < *it_rem) {
				prev = it_chunk;
				it_chunk = it_chunk->next;
			} else if (*it_rem < it_chunk->active) {
				++it_rem;
			} else {
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
					if constexpr (!unbound<T>) {
						auto const offset = it_chunk->range.offset(it_rem->first());
						std::destroy_n(&it_chunk->data[offset], it_rem->ucount());
					}

					if (maybe_split_range.has_value()) {
						// If two ranges were returned, split this chunk
						it_chunk->has_split_data = true;
						it_chunk->next =
							create_new_chunk(it_chunk->range, maybe_split_range.value(), it_chunk->data, it_chunk->next, false);
					}

					prev = it_chunk;
					it_chunk = it_chunk->next;
				}
			}
		}
	}

	// Removes transient components
	constexpr void process_remove_components() noexcept requires transient<T> {
		// All transient components are removed each cycle
		free_all_chunks();
	}
};
} // namespace ecs::detail

#endif // !ECS_DETAIL_COMPONENT_POOL_H

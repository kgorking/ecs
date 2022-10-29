#ifndef ECS_DETAIL_COMPONENT_POOL_H
#define ECS_DETAIL_COMPONENT_POOL_H

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

#ifdef _MSC_VER
	#define no_unique_address msvc::no_unique_address
#endif

template <typename ForwardIt, typename BinaryPredicate>
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

template <typename Cont, typename BinaryPredicate>
void combine_erase(Cont& cont, BinaryPredicate&& p) noexcept {
	auto const end = std_combine_erase(cont.begin(), cont.end(), static_cast<BinaryPredicate&&>(p));
	cont.erase(end, cont.end());
}

template <typename T, typename Alloc = std::allocator<T>>
class component_pool final : public component_pool_base {
private:
	static_assert(!is_parent<T>::value, "can not have pools of any ecs::parent<type>");

	struct chunk {
		chunk() noexcept = default;
		chunk(chunk const&) = delete;
		chunk(chunk&& other) noexcept
			: range{other.range}, active{other.active}, data{other.data}, owns_data{other.owns_data}, has_split_data{other.has_split_data} {
			other.data = nullptr;
		}
		chunk& operator=(chunk const&) = delete;
		chunk& operator=(chunk&& other) noexcept {
			range = other.range;
			active = other.active;
			data = other.data;
			other.data = nullptr;
			owns_data = other.owns_data;
			has_split_data = other.has_split_data;
			return *this;
		}
		chunk(entity_range range_, entity_range active_, T* data_ = nullptr, bool owns_data_ = false,
						bool has_split_data_ = false) noexcept
			: range(range_), active(active_), data(data_), owns_data(owns_data_), has_split_data(has_split_data_) {
		}

		~chunk() {
		}

		// The full range this chunk covers.
		entity_range range;

		// The partial range of active entities inside this chunk
		entity_range active;

		// The data for the full range of the chunk (range.count())
		T* data;

		// Signals if this chunk owns this data and should clean it up
		bool owns_data;

		// Signals if this chunk has been split
		bool has_split_data;

		bool operator<(chunk const& other) const {
			return active < other.active;
		}
	};
	// static_assert(sizeof(chunk) <= 32);

	//
	struct entity_empty {
		entity_range rng;
		entity_empty(entity_range r) noexcept : rng{r} {}
	};
	struct entity_data_member : entity_empty {
		T data;
		entity_data_member(entity_range r, T const& t) noexcept : entity_empty{r}, data(t) {}
		entity_data_member(entity_range r, T&& t) noexcept : entity_empty{r}, data(std::forward<T>(t)) {}
	};
	struct entity_span_member : entity_empty {
		std::span<const T> data;
		entity_span_member(entity_range r, std::span<const T> t) noexcept : entity_empty{r}, data(t) {}
	};
	struct entity_gen_member : entity_empty {
		std::function<T(entity_id)> data;
		entity_gen_member(entity_range r, std::function<T(entity_id)>&& t) noexcept
			: entity_empty{r}, data(std::forward<std::function<T(entity_id)>>(t)) {}
	};

	using entity_data = std::conditional_t<unbound<T>, entity_empty, entity_data_member>;
	using entity_span = std::conditional_t<unbound<T>, entity_empty, entity_span_member>;
	using entity_gen = std::conditional_t<unbound<T>, entity_empty, entity_gen_member>;

	using chunk_iter = typename std::vector<chunk>::iterator;
	using chunk_const_iter = typename std::vector<chunk>::const_iterator;

	std::vector<entity_range> ordered_active_ranges;
	std::vector<chunk> chunks;

	// Status flags
	bool components_added :1 = false;
	bool components_removed :1 = false;
	bool components_modified :1 = false;

	// Keep track of which components to add/remove each cycle
	[[no_unique_address]] tls::collect<std::vector<entity_data>, component_pool<T>> deferred_adds;
	[[no_unique_address]] tls::collect<std::vector<entity_span>, component_pool<T>> deferred_spans;
	[[no_unique_address]] tls::collect<std::vector<entity_gen>, component_pool<T>> deferred_gen;
	[[no_unique_address]] tls::collect<std::vector<entity_range>, component_pool<T>> deferred_removes;

	[[no_unique_address]] Alloc alloc;

public:
	component_pool() noexcept {
		if constexpr (global<T>) {
			chunks.emplace_back(entity_range::all(), entity_range::all(), nullptr, false, false);
			chunks.front().data = new T[1];
			ordered_active_ranges.push_back(entity_range::all());
		}
	}
	component_pool(component_pool const&) = delete;
	component_pool(component_pool&&) = delete;
	component_pool& operator=(component_pool const&) = delete;
	component_pool& operator=(component_pool&&) = delete;
	~component_pool() noexcept override {
		if (global<T>) {
			delete [] chunks.front().data;
		} else {
			free_all_chunks();
		}
	}

	// Add a span of component to a range of entities
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	void add_span(entity_range const range, std::span<const T> span) noexcept requires(!detail::unbound<T>) {
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
	void add(entity_range const range, T&& component) noexcept {
		if constexpr (tagged<T>) {
			deferred_adds.local().emplace_back(range);
		} else {
			deferred_adds.local().emplace_back(range, std::forward<T>(component));
		}
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	void add(entity_range const range, T const& component) noexcept {
		if constexpr (tagged<T>) {
			deferred_adds.local().emplace_back(range);
		} else {
			deferred_adds.local().emplace_back(range, component);
		}
	}

	// Return the shared component
	T& get_shared_component() noexcept requires global<T> {
		return chunks.front().data[0];
	}

	// Remove an entity from the component pool.
	void remove(entity_id const id) noexcept {
		remove({id, id});
	}

	// Remove an entity from the component pool.
	void remove(entity_range const range) noexcept {
		deferred_removes.local().push_back(range);
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	T* find_component_data(entity_id const id) noexcept requires(!global<T>) {
		return const_cast<T*>(std::as_const(*this).find_component_data(id));
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	T const* find_component_data(entity_id const id) const noexcept requires(!global<T>) {
		if (chunks.empty())
			return nullptr;

		auto const range_it = find_in_ordered_active_ranges({id, id});
		if (range_it != ordered_active_ranges.end() && range_it->contains(id)) {
			auto const chunk_it = chunks.begin() + ranges_dist(range_it);
			auto const offset = chunk_it->range.offset(id);
			return &chunk_it->data[offset];
		}

		return nullptr;
	}

	// Merge all the components queued for addition to the main storage,
	// and remove components queued for removal
	void process_changes() noexcept override {
		if constexpr (!global<T>) {
			process_remove_components();
			process_add_components();
		}
	}

	// Returns the number of active entities in the pool
	ptrdiff_t num_entities() const noexcept {
		ptrdiff_t count = 0;

		for (entity_range const r : ordered_active_ranges) {
			count += r.count();
		}

		return count;
	}

	// Returns the number of active components in the pool
	ptrdiff_t num_components() const noexcept {
		if constexpr (unbound<T>)
			return 1;
		else
			return num_entities();
	}

	// Returns the number of chunks in use
	ptrdiff_t num_chunks() const noexcept {
		return std::ssize(chunks);
	}

	chunk_const_iter get_head_chunk() const noexcept {
		return chunks.begin();
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
			// static constinit entity_range global_range = entity_range::all();
			// return entity_range_view{&global_range, 1};
			return ordered_active_ranges;
		} else {
			return ordered_active_ranges;
		}
	}

	// Returns true if an entity has a component in this pool
	bool has_entity(entity_id const id) const noexcept {
		return has_entity({id, id});
	}

	// Returns true if an entity range has components in this pool
	bool has_entity(entity_range const& range) const noexcept {
		auto const it = find_in_ordered_active_ranges(range);

		if (it == ordered_active_ranges.end())
			return false;

		return it->contains(range);
	}

	// Clear all entities from the pool
	void clear() noexcept override {
		// Remember if components was removed from the pool
		bool const is_removed = (!chunks.empty());

		// Clear all data
		free_all_chunks();
		deferred_adds.reset();
		deferred_spans.reset();
		deferred_gen.reset();
		deferred_removes.reset();
		ordered_active_ranges.clear();
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
	chunk_iter create_new_chunk(chunk_iter it_loc, entity_range const range, entity_range const active, T* data = nullptr,
								bool owns_data = true, bool split_data = false) noexcept {
		auto const dist = std::distance(chunks.begin(), it_loc);
		auto const range_it = ordered_active_ranges.begin() + dist;
		ordered_active_ranges.insert(range_it, active);

		return chunks.emplace(it_loc, range, active, data, owns_data, split_data);
	}

	chunk_iter create_new_chunk(chunk_iter loc, std::forward_iterator auto const& iter) noexcept {
		entity_range const r = iter->rng;
		chunk_iter c = create_new_chunk(loc, r, r);
		if constexpr (!unbound<T>) {
			c->data = alloc.allocate(r.ucount());
			construct_range_in_chunk(c, r, iter->data);
		}

		return c;
	}

	void free_chunk_data(chunk_iter c) noexcept {
		// Check for potential ownership transfer
		if (c->owns_data) {
			auto next = std::next(c);
			if (c->has_split_data && chunks.end() != next) {
				Expects(c->range == next->range);
				// transfer ownership
				next->owns_data = true;
			} else {
				if constexpr (!unbound<T>) {
					// Destroy active range
					std::destroy_n(c->data, c->active.ucount());

					// Free entire range
					alloc.deallocate(c->data, c->range.ucount());

					// Debug
					c->data = nullptr;
				}
			}
		}
	}

	[[nodiscard]]
	chunk_iter free_chunk(chunk_iter c) noexcept {
		free_chunk_data(c);
		return remove_range_to_chunk(c);
	}

	void free_all_chunks() noexcept {
		if constexpr (!global<T>) {
			auto chunk_it = chunks.begin();
			while (chunk_it != chunks.end()) {
				free_chunk_data(chunk_it);
				std::advance(chunk_it, 1);
			}
			chunks.clear();
			ordered_active_ranges.clear();
			set_data_removed();
		}
	}

	auto find_in_ordered_active_ranges(entity_range const rng) noexcept {
		return std::ranges::lower_bound(ordered_active_ranges, rng, std::less{});
	}
	auto find_in_ordered_active_ranges(entity_range const rng) const noexcept {
		return std::ranges::lower_bound(ordered_active_ranges, rng, std::less{});
	}

	ptrdiff_t ranges_dist(std::vector<entity_range>::const_iterator it) const noexcept {
		return std::distance(ordered_active_ranges.begin(), it);
	}

	// Removes a range and chunk from the map
	[[nodiscard]]
	chunk_iter remove_range_to_chunk(chunk_iter it) noexcept {
		auto const dist = std::distance(chunks.begin(), it);

		ordered_active_ranges.erase(ordered_active_ranges.begin() + dist);
		return chunks.erase(it);
	}

	// Updates a key in the range-to-chunk map
	void update_range_to_chunk_key(chunk_iter it, entity_range const update) noexcept {
		// auto it = find_in_ordered_active_ranges(old);
		//*it = update;
		auto const dist = std::distance(chunks.begin(), it);
		*(ordered_active_ranges.begin() + dist) = update;
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
		for (size_t i = 1; i < ordered_active_ranges.size(); ++i) {
			if (ordered_active_ranges[i - 1].overlaps(ordered_active_ranges[i]))
				return true;
		}

		return false;
	}

	static bool is_equal(T const& lhs, T const& rhs) noexcept requires std::equality_comparable<T> {
		return lhs == rhs;
	}
	static bool is_equal(T const& /*lhs*/, T const& /*rhs*/) noexcept requires tagged<T> {
		// Tags are empty, so always return true
		return true;
	}
	static bool is_equal(T const&, T const&) noexcept {
		// Type can not be compared, so always return false.
		// memcmp is a no-go because it also compares padding in types,
		// and it is not constexpr
		return false;
	}

	template <typename Data>
	void construct_range_in_chunk(chunk_iter c, entity_range range, Data const& comp_data) noexcept requires(!unbound<T>) {
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

	void fill_data_in_existing_chunk(chunk_iter& curr, entity_range r) noexcept {
		auto next = std::next(curr);

		// If split chunks are encountered, skip forward to the chunk closest to r
		if (curr->has_split_data) {
			while (chunks.end() != next && next->range.contains(r) && next->active < r) {
				std::advance(curr, 1);
				next = std::next(curr);
			}
		}

		if (curr->active.adjacent(r)) {
			// The two ranges are next to each other, so add the data to existing chunk
			entity_range active_range = entity_range::merge(curr->active, r);
			update_range_to_chunk_key(curr, active_range);
			curr->active = active_range;

			// Check to see if this chunk can be collapsed into 'prev'
			if (chunks.begin() != curr) {
				auto prev = std::next(curr, -1);
				if (prev->active.adjacent(curr->active)) {
					active_range = entity_range::merge(prev->active, curr->active);
					update_range_to_chunk_key(prev, active_range);
					prev = remove_range_to_chunk(prev);
					prev->active = active_range;

					curr = free_chunk(curr);
					next = std::next(curr);
				}
			}

			// Check to see if 'next' can be collapsed into this chunk
			if (chunks.end() != next) {
				if (curr->active.adjacent(next->active)) {
					active_range = entity_range::merge(curr->active, next->active);
					update_range_to_chunk_key(curr, active_range);
					next = free_chunk(next);

					curr->active = active_range;

					// split_data is true if the next chunk is also in the current range
					curr->has_split_data = (next != chunks.end()) && (curr->range == next->range);
				}
			}
		} else {
			// There is a gap between the two ranges, so split the chunk
			if (r < curr->active) {
				bool const curr_owns_data = curr->owns_data;
				curr->owns_data = false;
				curr = create_new_chunk(curr, curr->range, r, curr->data, curr_owns_data, true);
			} else {
				curr->has_split_data = true;
				curr = create_new_chunk(std::next(curr), curr->range, r, curr->data, false, false);
			}
		}
	}

	// Try to combine two ranges. With data
	static bool combiner_bound(entity_data& a, entity_data const& b) requires(!unbound<T>) {
		if (a.rng.adjacent(b.rng) && is_equal(a.data, b.data)) {
			a.rng = entity_range::merge(a.rng, b.rng);
			return true;
		} else {
			return false;
		}
	}

	// Try to combine two ranges. Without data
	static bool combiner_unbound(entity_data& a, entity_data const& b) requires(unbound<T>) {
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
	void process_add_components(std::vector<U> const& vec) noexcept {
		if (vec.empty()) {
			return;
		}

		// Do the insertions
		auto iter = vec.begin();
		auto curr = chunks.begin();

		// Fill in values
		while (iter != vec.end()) {
			if (chunks.empty()) {
				curr = create_new_chunk(curr, iter);
			} else {
				entity_range const r = iter->rng;

				// Move current chunk iterator forward
				auto next = std::next(curr);
				while (chunks.end() != next && next->range < r) {
					curr = next;
					std::advance(next, 1);
				}

				if (curr->range.overlaps(r)) {
					// Incoming range overlaps the current one, so add it into 'curr'
					fill_data_in_existing_chunk(curr, r);
					if constexpr (!unbound<T>) {
						construct_range_in_chunk(curr, r, iter->data);
					}
				} else if (curr->range < r) {
					// Incoming range is larger than the current one, so add it after 'curr'
					curr = create_new_chunk(std::next(curr), iter);
					// std::advance(curr, 1);
				} else if (r < curr->range) {
					// Incoming range is less than the current one, so add it before 'curr' (after 'prev')
					curr = create_new_chunk(curr, iter);
				}
			}

			++iter;
		}
	}

	// Add new queued entities and components to the main storage.
	void process_add_components() noexcept {
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
	void process_remove_components() noexcept {
		deferred_removes.for_each([this](std::vector<entity_range>& vec) {
			// Sort the ranges to remove
			std::sort(vec.begin(), vec.end());
			this->process_remove_components(vec);
		});
		deferred_removes.reset();

		// Update the state
		set_data_removed();
	}

	void process_remove_components(std::vector<entity_range>& removes) noexcept {
		chunk_iter it_chunk = chunks.begin();
		auto it_rem = removes.begin();

		while (it_chunk != chunks.end() && it_rem != removes.end()) {
			if (it_chunk->active < *it_rem) {
				std::advance(it_chunk, 1);
			} else if (*it_rem < it_chunk->active) {
				++it_rem;
			} else {
				//if (it_chunk->active == *it_rem) {
				if (it_rem->contains(it_chunk->active)) {
					// Delete the chunk and potentially its data
					it_chunk = free_chunk(it_chunk);
				} else {
					// remove partial range
					auto const [left_range, maybe_split_range] = entity_range::remove(it_chunk->active, *it_rem);

					// Update the active range
					update_range_to_chunk_key(it_chunk, left_range);
					it_chunk->active = left_range;

					// Destroy the removed components
					if constexpr (!unbound<T>) {
						auto const offset = it_chunk->range.offset(it_rem->first());
						std::destroy_n(&it_chunk->data[offset], it_rem->ucount());
					}

					if (maybe_split_range.has_value()) {
						// If two ranges were returned, split this chunk
						it_chunk->has_split_data = true;
						it_chunk = create_new_chunk(std::next(it_chunk), it_chunk->range, maybe_split_range.value(), it_chunk->data, false);
					} else {
						std::advance(it_chunk, 1);
					}
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

#endif // !ECS_DETAIL_COMPONENT_POOL_H

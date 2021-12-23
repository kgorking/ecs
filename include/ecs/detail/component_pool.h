#ifndef ECS_COMPONENT_POOL
#define ECS_COMPONENT_POOL

#include <cstring> // for memcmp
#include <functional>
#include <tuple>
#include <type_traits>
#include <vector>

#include "tls/collect.h"

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

	struct level_0 {
		entity_range range;
		std::vector<T> data;
	};
	struct level_1 {
		static constexpr std::chrono::seconds time_to_upgrade{3};

		entity_range range;  // the total range allocated
		entity_range active; // the range in use
		std::vector<T> data;
		time_point last_modified; // modified here means back/front insertion/deletion
	};
	struct level_2 {
		static constexpr std::chrono::seconds time_to_upgrade = 4 * level_1::time_to_upgrade;

		entity_range range;
		std::vector<uint32_t> skips;
		std::vector<T> data;
		time_point last_modified;

		void init_skips(entity_range r1, entity_range r2) {
			//     r1   r2
			// ---***---**--- range
			// #1    #2   #3

			// part 1
			auto count = r1.first() - range.first();
			auto it = skips.begin();
			while (count > 0) {
				*it = count;
				++it;
				--count;
			}

			// part 2
			count = r2.first() - r1.last() - 1;
			it = skips.begin() + r1.last() + 1;
			while (count > 0) {
				*it = count;
				++it;
				--count;
			}

			// part 3
			count = range.last() - r2.last();
			it = skips.begin() + r2.last() + 1;
			while (count > 0) {
				*it = count;
				++it;
				--count;
			}
		}
	};

	// The component levels
	std::vector<level_0> l0;
	std::vector<level_1> l1;
	std::vector<level_2> l2;

	// The total range of entities in the levels. Ranges are only valid if the level is not empty.
	// Used for broad-phase testing
	entity_range l0_range{entity_range::all()};
	entity_range l1_range{entity_range::all()};
	entity_range l2_range{entity_range::all()};

	// Cache of all active ranges
	std::vector<entity_range> cached_ranges;

	// Keep track of which components to add/remove each cycle
	using entity_data = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, T>>;
	using entity_init = std::tuple<entity_range, std::function<T(entity_id)>>;
	tls::collect<std::vector<entity_data>, component_pool<T>> deferred_adds;
	tls::collect<std::vector<entity_init>, component_pool<T>> deferred_inits;
	tls::collect<std::vector<entity_range>, component_pool<T>> deferred_removes;

	// Status flags
	bool components_added = false;
	bool components_removed = false;
	bool components_modified = false;

public:

	// Add a component to a range of entities, initialized by the supplied user function
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	template <typename Fn>
	void add_init(entity_range const range, Fn&& init) {
		// Add the range and function to a temp storage
		deferred_inits.local().emplace_back(range, std::forward<Fn>(init));
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
		remove_range({id, id});
	}

	// Remove an entity from the component pool. This logically removes the component from the
	// entity.
	void remove_range(entity_range const range) {
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
		entity_range const r{id, id};

		// search in level 0
		if (!l0.empty()) {
			auto const it = std::ranges::lower_bound(l0, r, std::less{}, &level_0::range);
			if (it != l0.end() && it->range.contains(id)) {
				auto const offset = it->range.offset(id);
				return &it->data[offset];
			}
		}

		// search in level 1
		if (!l1.empty()) {
			auto const it = std::ranges::lower_bound(l1, r, std::less{}, &level_1::range);
			if (it != l1.end() && it->active.contains(id)) {
				auto const offset = it->range.offset(id);
				return &it->data[offset];
			}
		}

		// search in level 2
		if (!l2.empty()) {
			auto const it = std::ranges::lower_bound(l2, r, std::less{}, &level_2::range);
			if (it != l2.end() && it->range.contains(id)) {
				auto const offset = it->range.offset(id);
				if (it->skips[offset] == 0)
					return &it->data[offset];
			}
		}

		return nullptr;
	}

	// Merge all the components queued for addition to the main storage,
	// and remove components queued for removal
	void process_changes() override {
		process_remove_components();

		size_t const l0_size_start = l0.size();
		size_t const l2_size_start = l2.size();

		process_add_components();

		handle_levelupgrades();
		// TODO? collapse_adjacent_ranges()

		// Sort it
		// New ranges are never added to l1, but existing l1
		// can be downgraded to l2.
		if (l0_size_start == 0 && !l0.empty()) {
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
		}

		update_cached_ranges();
	}

	// Returns the number of active entities in the pool
	size_t num_entities() const {
		size_t count = 0;
		for (level_0 const& t : l0)
			count += t.range.ucount();
		for (level_1 const& t : l1)
			count += t.active.ucount();
		for (level_2 const& t : l2)
			for (auto skip : t.skips)
				count += (skip == 0);
		return count;
	}

	// Returns the number of active components in the pool
	size_t num_components() const {
		if constexpr (unbound<T>)
			return 1;
		else
			return num_entities();
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
		// search in level 0
		if (!l0.empty()) {
			auto const it = std::ranges::lower_bound(l0, range, std::less{}, &level_0::range);
			if (it != l0.end() && it->range.contains(range)) {
				return true;
			}
		}

		// search in level 1
		if (!l1.empty()) {
			auto const it = std::ranges::lower_bound(l1, range, std::less{}, &level_1::range);
			if (it != l1.end() && it->active.contains(range)) {
				return true;
			}
		}

		// search in level 2
		if (!l2.empty()) {
			auto const it = std::ranges::lower_bound(l2, range, std::less{}, &level_2::range);
			if (it != l2.end() && it->range.contains(range)) {
				auto const offset_start = it->range.offset(range.first());
				auto const offset_end = it->range.offset(range.first());

				auto const tester = [](auto skip) {
					return skip == 0;
				};

				if (std::all_of(&it->skips[offset_start], &it->skips[offset_end], tester))
					return true;
			}
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
		bool const is_removed = !l0.empty() || !l1.empty() || !l2.empty();

		// Clear the pool
		l0.clear();
		l1.clear();
		l2.clear();
		deferred_adds.reset();
		deferred_inits.reset();
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
	// Flag that components has been added
	void set_data_added() {
		components_added = true;
	}

	// Flag that components has been removed
	void set_data_removed() {
		components_removed = true;
	}

	void handle_levelupgrades() {
		auto const now = clock::now();

		for (auto it = l1.begin(); it != l1.end(); ) {
			if (now - it->last_modified >= level_1::time_to_upgrade) {
				l0.push_back(upgrade_l1_to_l0(*it));
				it = l1.erase(it);
			} else {
				++it;
			}
		}

		// l2
	}

	void update_cached_ranges() {
		if (!components_added && !components_removed)
			return;

		cached_ranges.clear();

		// Find all ranges
		if (!l0.empty()) {
			l0_range = l0[0].range;
			for (size_t i = 0; i < l0.size(); ++i) {
				auto const& level = l0[i];
				l0_range = entity_range::overlapping(l0_range, level.range);

				cached_ranges.push_back(level.range);
			}
		}

		if (!l1.empty()) {
			l1_range = l1[0].range;
			for (size_t i = 0; i < l1.size(); ++i) {
				auto const& level = l1[i];
				l1_range = entity_range::overlapping(l1_range, level.active);

				cached_ranges.push_back(level.active);
			}
		}

		if (!l2.empty()) {
			l2_range = l2[0].range;
			for (size_t i = 0; i < l2.size(); ++i) {
				auto const& level = l2[i];
				l2_range = entity_range::overlapping(l2_range, level.range);

				auto it = level.skips.begin();
				do {
					it += *it;
					if (it == level.skips.end())
						break;

					entity_type first = static_cast<entity_type>(level.range.first() + std::distance(level.skips.begin(), it));

					while (it != level.skips.end() && *it == 0) {
						++it;
					}

					entity_id last = static_cast<entity_type>(level.range.first() + std::distance(level.skips.begin(), it) - 1);
					cached_ranges.push_back({first, last});
				} while (it != level.skips.end());
			}
		}
	}

	// Verify the 'add*' functions precondition.
	// An entity can not have more than one of the same component
	static bool has_duplicate_entities(auto const& vec) {
		return vec.end() != std::adjacent_find(vec.begin(), vec.end(),
												[](auto const& l, auto const& r) { return l.range == r.range; });
	};

	static T get_data(entity_id /*id*/, entity_data& ed) {
		return std::get<1>(ed);
	}
	static T get_data(entity_id id, entity_init& ed) {
		return std::get<1>(ed)(id);
	}

	void add_to_l0(entity_range r, entity_data&) requires unbound<T> {
		l0.push_back(level_0{r, {}});
	}

	void add_to_l0(entity_range r, entity_data& data) {
		l0.push_back(level_0{r, std::vector<T>(r.ucount(), std::get<1>(data))}); // emplace_back is broken in clang -_-
	}

	void add_to_l0(entity_range r, entity_init& init) {
		std::vector<T> leveldata;
		leveldata.reserve(r.ucount());
		for (entity_id ent : r) {
			leveldata.push_back(std::get<1>(init)(ent));
		}
		l0.push_back(level_0{r, std::move(leveldata)}); // emplace_back is broken in clang -_-
	}

	// Add new queued entities and components to the main storage.
	void process_add_components() {
		auto const processor = [this](auto& vec) {
			for (auto& data : vec) {
				entity_range const r = std::get<0>(data);

				if (!l1.empty() && l1_range.overlaps(r)) {
					// The new range may exist in a level_ 1 range
					auto const it = std::ranges::lower_bound(l1, r, std::less{}, &level_1::range);
					if (it != l1.end() && it->range.contains(r)) {
						level_1& level_1 = *it;

						Expects(!level_1.active.overlaps(r)); // entity already has a component

						// If the ranges are adjacent, grow the active range
						if (level_1.active.adjacent(r)) {
							level_1.active = entity_range::merge(level_1.active, r);
							if constexpr (!detail::unbound<T>) {
								for (auto ent = r.first(); ent <= r.last(); ++ent) {
									auto const offset = level_1.range.offset(ent);
									level_1.data[offset] = get_data(ent, data);
								}
							}
						} else {
							// The ranges are separate, so downgrade to l2
							l2.push_back(downgrade_l1_to_l2(std::move(level_1), level_1.active, r));
							l1.erase(it);
						}

						// This range is handled, so continue on to the next one in 'vec'
						continue;
					}
				}

				if (!l2.empty() && l2_range.overlaps(r)) {
					// The new range may exist in a level_ 2 range
					auto const it = std::ranges::lower_bound(l2, r, std::less{}, &level_2::range);
					if (it != l2.end() && it->range.contains(r)) {
						level_2& level_2 = *it;

						for (auto ent = r.first(); ent <= r.last(); ++ent) {
							auto offset = level_2.range.offset(ent);

							if constexpr (!detail::unbound<T>) {
								level_2.data[offset] = get_data(ent, data);
							}

							auto const current_skips = level_2.skips[offset];
							Expects(level_2.skips[offset] != 0); // entity already has a component
							while (level_2.skips[offset] != 0) {
								level_2.skips[offset] -= current_skips;
								if (offset == 0)
									break;
								offset -= 1;
							}
						}

						// This range is handled, so continue on to the next one in 'vec'y
						continue;
					}
				}

				// range has not been handled yet, so Kobe it into l0
				add_to_l0(r, data);
			}
			vec.clear();
		};

		// Move new components into the pool
		deferred_adds.for_each(processor);
		deferred_inits.for_each(processor);

		// Check it
		Expects(false == has_duplicate_entities(l0));

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

		// Remove level_ 0 ranges, or downgrade them to level_ 1 or 2
		if (!l0.empty()) {
			process_remove_components_level_0(vec);
			//std::ranges::sort(l0, std::less{}, &level_0::range);
		}

		// Remove level_ 1 ranges, or downgrade them to level_ 2
		if (!l1.empty()) {
			process_remove_components_level_1(vec);
			//std::ranges::sort(l1, std::less{}, &level_1::range);
		}

		// Remove level_ 2 ranges
		if (!l2.empty()) {
			process_remove_components_level_2(vec);
			//std::ranges::sort(l2, std::less{}, &level_2::range);
		}

		// Update the state
		set_data_removed();
	}

	void process_remove_components_level_0(std::vector<entity_range>& removes) {
		auto it_l0 = l0.begin();
		auto it_rem = removes.begin();

		while (it_l0 != l0.end() && it_rem != removes.end()) {
			if (it_l0->range < *it_rem)
				++it_l0;
			else if (*it_rem < it_l0->range)
				++it_rem;
			else {
				if (it_l0->range == *it_rem) {
					// remove an entire range
					// todo: move to free-store?
				} else {
					// remove partial range
					auto const [left_range, maybe_split_range] = entity_range::remove(it_l0->range, *it_rem);
					
					// If two ranges were returned, downgrade to level_ 2
					if (maybe_split_range.has_value()) {
						l2.push_back(downgrade_l0_to_l2(std::move(*it_l0), left_range, maybe_split_range.value()));
					} else {
						// downgrade to level_ 1 with a partial range
						l1.push_back(downgrade_l0_to_l1(std::move(*it_l0), left_range));
					}
				}

				// TODO re-use?
				it_l0 = l0.erase(it_l0);
				it_rem = removes.erase(it_rem);
			}
		}
	}

	void process_remove_components_level_1(std::vector<entity_range>& removes) {
		auto it_l1 = l1.begin();
		auto it_rem = removes.begin();

		while (it_l1 != l1.end() && it_rem != removes.end()) {
			if (it_l1->range < *it_rem)
				++it_l1;
			else if (*it_rem < it_l1->range)
				++it_rem;
			else {
				if (it_l1->active == *it_rem || it_l1->range.contains(*it_rem)) {
					// remove an entire range
					// todo: move to free-store?
					it_l1 = l1.erase(it_l1);
				} else {
					// remove partial range
					auto const [left_range, maybe_split_range] = entity_range::remove(it_l1->range, *it_rem);
					
					// If two ranges were returned, downgrade to level_ 2
					if (maybe_split_range.has_value()) {
						l2.push_back(downgrade_l1_to_l2(std::move(*it_l1), left_range, maybe_split_range.value()));
						it_l1 = l1.erase(it_l1);
					} else {
						// adjust the active range
						it_l1->active = left_range;

						// update the modification time
						it_l1->last_modified = clock::now();
					}
				}

				it_rem = removes.erase(it_rem);
			}
		}
	}

	void process_remove_components_level_2(std::vector<entity_range>& removes) {
		auto it_l2 = l2.begin();
		auto it_rem = removes.begin();

		while (it_l2 != l2.end() && it_rem != removes.end()) {
			if (it_l2->range < *it_rem)
				++it_l2;
			else if (*it_rem < it_l2->range)
				++it_rem;
			else {
				if (it_l2->range == *it_rem) {
					// remove an entire range
					// todo: move to free-store?
					it_l2 = l2.erase(it_l2);
				} else {
					// remove partial range
					// update the skips
					for (entity_type i = it_rem->first(); i <= it_rem->last(); ++i) {
						auto offset = it_l2->range.offset(i);

						Expects(it_l2->skips[offset] == 0); // trying to remove already removed entity

						// Skip this one entity
						auto skip_amount = 1;

						// If the entity to the right is also skipped,
						// added their skip-amount to our own to maintain
						// the chain of skip values.
						if(offset != it_l2->skips.size() - 1)
							skip_amount += it_l2->skips[offset + 1];

						// Store the skip
						it_l2->skips[offset] = skip_amount;

						// Increment previous skips if needed
						if (offset > 0) {
							auto j = offset - 1;
							while (it_l2->skips[j] > 0) {
								it_l2->skips[j] += skip_amount;
								j -= 1;
							}
						}
					}

					// If whole range is skipped, just erase it
					if (it_l2->skips[0] == it_l2->range.ucount())
						it_l2 = l2.erase(it_l2);
					else {
						// otherwise update the modification time
						it_l2->last_modified = clock::now();
					}
				}

				//it_rem = removes.erase(it_rem);
				++it_rem;
			}
		}
	}

	// Removes transient components
	void process_remove_components() requires transient<T> {
		// Transient components are removed each cycle
		l0.clear();
		l1.clear();
		l2.clear();
		set_data_removed();
	}

	level_1 downgrade_l0_to_l1(level_0 level_0, entity_range new_range) {
		return level_1{level_0.range, new_range, std::move(level_0.data), clock::now()};
	}

	level_2 downgrade_l0_to_l2(level_0 level_0, entity_range r1, entity_range r2) {
		level_2 level_2{level_0.range, std::vector<uint32_t>(level_0.data.size(), uint32_t{0}), std::move(level_0.data), clock::now()};
		level_2.init_skips(r1, r2);
		return level_2;
	}

	level_2 downgrade_l1_to_l2(level_1 level_1, entity_range r1, entity_range r2) {
		level_2 level_2{level_1.range, std::vector<uint32_t>(level_1.data.size(), uint32_t{0}), std::move(level_1.data), clock::now()};
		level_2.init_skips(r1, r2);
		return level_2;
	}

	level_0 upgrade_l1_to_l0(level_1& level_1) {
		std::vector<T> stuff;

		auto const off_first = level_1.range.offset(level_1.active.first());
		auto const off_last = level_1.range.offset(level_1.active.last());
		stuff.assign(level_1.data.begin() + off_first, level_1.data.end() + off_last);

		return level_0{level_1.active, std::move(stuff)};
	}
};
} // namespace ecs::detail

#endif // !ECS_COMPONENT_POOL

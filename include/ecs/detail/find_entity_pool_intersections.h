#ifndef ECS_FIND_ENTITY_POOL_INTERSECTIONS_H
#define ECS_FIND_ENTITY_POOL_INTERSECTIONS_H

#include "entity_range.h"
#include "system_defs.h"
#include <array>
#include <vector>

namespace ecs::detail {

template <class Component, typename TuplePools>
void pool_intersect(std::vector<entity_range>& ranges, TuplePools const& pools) {
	using T = std::remove_cvref_t<Component>;
	using iter1 = typename std::vector<entity_range>::iterator;
	using iter2 = typename entity_range_view::iterator;

	// Skip globals and parents
	if constexpr (detail::global<T>) {
		// do nothing
	} else if constexpr (detail::is_parent<T>::value) {
		auto const ents = get_pool<parent_id>(pools).get_entities();
		ranges = intersect_ranges_iter(iter_pair<iter1>{ranges.begin(), ranges.end()}, iter_pair<iter2>{ents.begin(), ents.end()});
	} else if constexpr (std::is_pointer_v<T>) {
		// do nothing
	} else {
		// ranges = intersect_ranges(ranges, get_pool<T>(pools).get_entities());
		auto const ents = get_pool<T>(pools).get_entities();
		ranges = intersect_ranges_iter(iter_pair<iter1>{ranges.begin(), ranges.end()}, iter_pair<iter2>{ents.begin(), ents.end()});
	}
}

template <class Component, typename TuplePools>
void pool_difference(std::vector<entity_range>& ranges, TuplePools const& pools) {
	using T = std::remove_cvref_t<Component>;

	if constexpr (std::is_pointer_v<T>) {
		using NoPtr = std::remove_pointer_t<T>;

		if constexpr (detail::is_parent<NoPtr>::value) {
			ranges = difference_ranges(ranges, get_pool<parent_id>(pools).get_entities());
		} else {
			ranges = difference_ranges(ranges, get_pool<NoPtr>(pools).get_entities());
		}
	}
}

// Find the intersection of the sets of entities in the specified pools
template <class FirstComponent, class... Components, typename TuplePools>
std::vector<entity_range> find_entity_pool_intersections(TuplePools const& pools) {
	std::vector<entity_range> ranges{entity_range::all()};

	if constexpr (std::is_same_v<entity_id, FirstComponent>) {
		(pool_intersect<Components, TuplePools>(ranges, pools), ...);
		(pool_difference<Components, TuplePools>(ranges, pools), ...);
	} else {
		pool_intersect<FirstComponent, TuplePools>(ranges, pools);
		(pool_intersect<Components, TuplePools>(ranges, pools), ...);

		pool_difference<FirstComponent, TuplePools>(ranges, pools);
		(pool_difference<Components, TuplePools>(ranges, pools), ...);
	}

	return ranges;
}

template <class ComponentList, typename TuplePools>
auto get_pool_iterators(TuplePools pools) {
	using iter = iter_pair<entity_range_view::iterator>;

	return apply_type<ComponentList>([&]<typename... Components>() {
		return std::array<iter, sizeof...(Components)>{
			iter{get_pool<std::conditional_t<detail::is_parent<Components>::value, parent_id, Components>>(pools).get_entities().begin(),
				 get_pool<std::conditional_t<detail::is_parent<Components>::value, parent_id, Components>>(pools).get_entities().end()}...};
	});
}


// Find the intersection of the sets of entities in the specified pools
template <class ComponentList, typename TuplePools, typename F>
void find_entity_pool_intersections_cb(TuplePools pools, F callback) {
	static_assert(0 < type_list_size<ComponentList>, "Empty component list supplied");

	// The type of iterators used
	using iter = iter_pair<entity_range_view::iterator>;

	// Split the type_list into filters and non-filters (regular components)
	using SplitPairList = split_types_if<ComponentList, std::is_pointer>;
	auto iter_filters = get_pool_iterators<typename SplitPairList::first>(pools);
	auto iter_components = get_pool_iterators<typename SplitPairList::second>(pools);

	auto const done = [](auto it) {
		return it.curr == it.end;
	};

	while (!std::any_of(iter_components.begin(), iter_components.end(), done)) {
		// Get the starting range to test other ranges against
		entity_range curr_range = *iter_components[0].curr;

		// Find all intersections
		if constexpr (type_list_size<typename SplitPairList::second> == 1) {
			iter_components[0].curr += 1;
		} else {
			bool intersection_found = false;
			for (size_t i = 1; i < iter_components.size(); ++i) {
				auto& it_a = iter_components[i - 1];
				auto& it_b = iter_components[i];

				if (done(it_a) /*|| done(it_b)*/)
					break;

				if (curr_range.overlaps(*it_b.curr)) {
					curr_range = entity_range::intersect(curr_range, *it_b.curr);
					intersection_found = true;
				}

				if (it_a.curr->last() < it_b.curr->last()) {
					// range a is inside range b, move to
					// the next range in a
					++it_a.curr;
				} else if (it_b.curr->last() < it_a.curr->last()) {
					// range b is inside range a,
					// move to the next range in b
					++it_b.curr;
				} else {
					// ranges are equal, move to next ones
					++it_a.curr;
					++it_b.curr;
				}
			}

			if (!intersection_found)
				continue;
		}

		// Filter the range, if needed
		if constexpr (type_list_size<typename SplitPairList::first> > 0) {
			// Sort the filters
			std::sort(iter_filters.begin(), iter_filters.end(), [](auto const& a, auto const& b) {
				return *a.curr < *b.curr;
			});

			bool completely_filtered = false;
			for (iter& it : iter_filters) {
				// If this filter has reached its end, skip ahead to next filter
				if (done(it))
					continue;

				if (it.curr->contains(curr_range)) {
					// 'curr_range' is contained entirely in filter range,
					// which means that it will not be sent to the callback
					completely_filtered = true;
					break;
				} else if (curr_range < *it.curr) {
					// The whole 'curr_range' is before the filter, so don't touch it
				} else if (*it.curr < curr_range) {
					// The filter precedes the range, so advance it
					it.curr++;
				} else {
					// The two ranges overlap
					auto const res = entity_range::remove(curr_range, *it.curr);

					if (res.second) {
						// 'curr_range' was split in two by the filter.
						// Send the first range and update
						// 'curr_range' to be the second range
						callback(res.first);
						curr_range = *res.second;

						it.curr++;
					} else {
						// The result is an endpiece, so update the current range.
						// The next filter might remove more from 'curr_range'
						curr_range = res.first;

						if (curr_range.first() >= it.curr->first()) {
							++it.curr;
						}
					}
				}
			}

			if (!completely_filtered)
				callback(curr_range);
		} else {
			// No filters on this range, so send
			callback(curr_range);
		}
	}
}

} // namespace ecs::detail

#endif // !ECS_FIND_ENTITY_POOL_INTERSECTIONS_H

#ifndef ECS_FIND_ENTITY_POOL_INTERSECTIONS_H
#define ECS_FIND_ENTITY_POOL_INTERSECTIONS_H

#include "entity_range.h"
#include "system_defs.h"
#include <array>
#include <vector>

namespace ecs::detail {

template <typename Component, typename TuplePools>
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

template <typename Component, typename TuplePools>
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
template <typename FirstComponent, typename... Components, typename TuplePools>
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

template <typename ComponentList, typename Pools>
auto get_pool_iterators([[maybe_unused]] Pools pools) {
	if constexpr (type_list_size<ComponentList> > 0) {
		return apply_type<ComponentList>([&]<typename... Components>() {
			return std::to_array({get_pool<Components>(pools).get_entities()...});
		});
	} else {
		return std::array<stride_view<0,char const>, 0>{};
	}
}


// Find the intersection of the sets of entities in the specified pools
template <typename ComponentList, typename Pools, typename F>
void find_entity_pool_intersections_cb(Pools pools, F callback) {
	static_assert(0 < type_list_size<ComponentList>, "Empty component list supplied");

	// Split the type_list into filters and non-filters (regular components)
	using SplitPairList = split_types_if<ComponentList, std::is_pointer>;
	auto iter_filters = get_pool_iterators<typename SplitPairList::first>(pools);
	auto iter_components = get_pool_iterators<typename SplitPairList::second>(pools);

	// Sort the filters
	std::sort(iter_filters.begin(), iter_filters.end(), [](auto const& a, auto const& b) {
		return *a.current() < *b.current();
	});

	// helper lambda to test if an iterator has reached its end
	auto const done = [](auto it) {
		return it.done();
	};

	while (!std::any_of(iter_components.begin(), iter_components.end(), done)) {
		// Get the starting range to test other ranges against
		entity_range curr_range = *iter_components[0].current();

		// Find all intersections
		if constexpr (type_list_size<typename SplitPairList::second> == 1) {
			iter_components[0].next();
		} else {
			bool intersection_found = false;
			for (size_t i = 1; i < iter_components.size(); ++i) {
				auto& it_a = iter_components[i - 1];
				auto& it_b = iter_components[i];

				if (curr_range.overlaps(*it_b.current())) {
					curr_range = entity_range::intersect(curr_range, *it_b.current());
					intersection_found = true;
				}

				if (it_a.current()->last() < it_b.current()->last()) {
					// range a is inside range b, move to
					// the next range in a
					it_a.next();
					if (it_a.done())
						break;

				} else if (it_b.current()->last() < it_a.current()->last()) {
					// range b is inside range a,
					// move to the next range in b
					it_b.next();
				} else {
					// ranges are equal, move to next ones
					it_a.next();
					if (it_a.done())
						break;

					it_b.next();
				}
			}

			if (!intersection_found)
				continue;
		}

		// Filter the range, if needed
		if constexpr (type_list_size<typename SplitPairList::first> > 0) {
			bool completely_filtered = false;
			for (auto& it : iter_filters) {
				while(!done(it)) {
					if (it.current()->contains(curr_range)) {
						// 'curr_range' is contained entirely in filter range,
						// which means that it will not be sent to the callback
						completely_filtered = true;
						break;
					} else if (curr_range < *it.current()) {
						// The whole 'curr_range' is before the filter, so don't touch it
						break;
					} else if (*it.current() < curr_range) {
						// The filter precedes the range, so advance it and restart
						it.next();
						continue;
					} else {
						// The two ranges overlap
						auto const res = entity_range::remove(curr_range, *it.current());

						if (res.second) {
							// 'curr_range' was split in two by the filter.
							// Send the first range and update
							// 'curr_range' to be the second range
							callback(res.first);
							curr_range = *res.second;

							it.next();
						} else {
							// The result is an endpiece, so update the current range.
							// The next filter might remove more from 'curr_range'
							curr_range = res.first;

							if (curr_range.first() >= it.current()->first()) {
								it.next();
							}
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

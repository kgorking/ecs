#ifndef ECS_FIND_ENTITY_POOL_INTERSECTIONS_H
#define ECS_FIND_ENTITY_POOL_INTERSECTIONS_H

#include "../entity_range.h"
#include "system_defs.h"
#include <vector>

namespace ecs::detail {

template <class Component, typename TuplePools>
void pool_intersect(std::vector<entity_range>& ranges, TuplePools const& pools) {
	using T = std::remove_cvref_t<Component>;

	// Skip globals and parents
	if constexpr (detail::global<T>) {
		// do nothing
	} else if constexpr (detail::is_parent<T>::value) {
		ranges = intersect_ranges(ranges, get_pool<parent_id>(pools).get_entities());
	} else if constexpr (std::is_pointer_v<T>) {
		// do nothing
	} else {
		ranges = intersect_ranges(ranges, get_pool<T>(pools).get_entities());
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

} // namespace ecs::detail

#endif // !ECS_FIND_ENTITY_POOL_INTERSECTIONS_H

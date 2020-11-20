#ifndef __FIND_ENTITY_POOL_INTERSECTIONS_H
#define __FIND_ENTITY_POOL_INTERSECTIONS_H

#include <vector>
#include "../entity_range.h"
#include "system_defs.h"

namespace ecs::detail {

    template<class Component, typename TuplePools>
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

    template<class Component, typename TuplePools>
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
    template<class FirstComponent, class... Components, typename TuplePools>
    std::vector<entity_range> find_entity_pool_intersections(TuplePools const& pools) {
        auto const ent_view = std::get<0>(pools)->get_entities();
        std::vector<entity_range> ranges{ent_view.begin(), ent_view.end()};

        (pool_intersect<Components, TuplePools>(ranges, pools), ...);

        pool_difference<FirstComponent, TuplePools>(ranges, pools);
        (pool_difference<Components, TuplePools>(ranges, pools), ...);

        return ranges;
    }

} // namespace ecs::detail

#endif // !__FIND_ENTITY_POOL_INTERSECTIONS_H

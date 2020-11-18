#ifndef __FIND_ENTITY_POOL_INTERSECTIONS_H
#define __FIND_ENTITY_POOL_INTERSECTIONS_H

#include <vector>
#include "../entity_range.h"
#include "system_defs.h"

namespace ecs::detail {

    template<typename T, typename Pools>
    void pool_intersect(std::vector<entity_range>& ranges, Pools const& pools) {
        // Skip globals and parents
        if constexpr (detail::global<T> /*|| detail::is_parent<T>*/)
            return;

        if constexpr (std::is_pointer_v<T>) {
            ranges = difference_ranges(ranges, get_pool<std::remove_pointer_t<T>>(pools).get_entities());
        } else {
            ranges = intersect_ranges(ranges, get_pool<T>(pools).get_entities());
        }
    }


    // Find the intersection of the sets of entities in the specified pools
    template<typename FT, typename... RT>
    std::vector<entity_range> find_entity_pool_intersections(tup_pools<FT, RT...> pools) {

        std::vector<entity_range> ranges{
            std::get<0>(pools)->get_entities().begin(), std::get<0>(pools)->get_entities().end()};

        (..., pool_intersect<std::remove_cvref_t<RT>>(ranges, pools));

        return ranges;
    }

} // namespace ecs::detail

#endif // !__FIND_ENTITY_POOL_INTERSECTIONS_H

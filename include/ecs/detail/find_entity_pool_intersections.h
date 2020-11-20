#ifndef __FIND_ENTITY_POOL_INTERSECTIONS_H
#define __FIND_ENTITY_POOL_INTERSECTIONS_H

#include <vector>
#include "../entity_range.h"
#include "system_defs.h"

namespace ecs::detail {

    template<typename T>
    struct nested_type_detect; // primary template

    template<template<class> class Parent, class Nested> // partial specialization
    struct nested_type_detect<Parent<Nested>> {
        using type = Nested;
    };

    template<typename T>
    using component_pool_type = typename nested_type_detect<std::remove_pointer_t<T>>::type;

    template<int Index, typename Tuple>
    void pool_intersect(std::vector<entity_range>& ranges, Tuple const& pools) {
        if constexpr (Index == std::tuple_size_v<Tuple>) {
            return;
        } else {
            using PoolType = std::remove_pointer_t<std::tuple_element_t<Index, Tuple>>;
            using T = component_pool_type<PoolType>;

            // Skip globals and parents
            if constexpr (detail::global<T> /*|| detail::is_parent<T>*/) {
                // do nothing
            } else if constexpr (std::is_pointer_v<T>) {
                ranges = difference_ranges(ranges, get_pool<std::remove_pointer_t<T>>(pools).get_entities());
            } else {
                ranges = intersect_ranges(ranges, get_pool<T>(pools).get_entities());
            }

            // Go to next type in the tuple
            pool_intersect<Index + 1>(ranges, pools);
        }
    }


    // Find the intersection of the sets of entities in the specified pools
    template<typename Tuple>
    std::vector<entity_range> find_entity_pool_intersections(Tuple const& pools) {
        if constexpr (0 == std::tuple_size_v<Tuple>) {
            return {};
        } else {
            std::vector<entity_range> ranges{
                entity_range{std::numeric_limits<entity_type>::min(), std::numeric_limits<entity_type>::max()}};

            pool_intersect<0>(ranges, pools);

            return ranges;
        }
    }

} // namespace ecs::detail

#endif // !__FIND_ENTITY_POOL_INTERSECTIONS_H

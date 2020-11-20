#ifndef __SYSTEM_DEFS_H_
#define __SYSTEM_DEFS_H_

// Contains definitions that are used by the system- and builder classes
#include "component_pool.h"
#include "verification.h"
#include "parent_id.h"

namespace ecs::detail {
    // Alias for stored pools
    template<class T>
    using pool = component_pool<std::remove_pointer_t<std::remove_cvref_t<T>>>* const;

    // If given a parent, convert to detail::parent_id, otherwise do nothing
    template<typename T>
    using reduce_parent_t = std::conditional_t<is_parent<T>::value, parent_id, T>;

    // Helper to extract the parent types
    template<typename T>
    struct parent_type_detect; // primary template

    template<template<class...> class Parent, class... ParentComponents> // partial specialization
    struct parent_type_detect<Parent<ParentComponents...>> {
        using type = std::tuple<ParentComponents...>;
    };

    // Helper to extract the parent pool types
    template<typename T>
    struct parent_pool_detect; // primary template

    template<template<class...> class Parent, class... ParentComponents> // partial specialization
    struct parent_pool_detect<Parent<ParentComponents...>> {
        using type = std::tuple<pool<ParentComponents>...>;
    };

    template<typename T>
    using parent_types_tuple_t = typename parent_type_detect<T>::type;
    template<typename T>
    using parent_pool_tuple_t = typename parent_pool_detect<T>::type;


    // Get a component pool from a component pool tuple
    template<typename Component, typename Pools>
    component_pool<Component>& get_pool(Pools const& pools) {
        return *std::get<pool<Component>>(pools);
    }

    // Get an entities component from a component pool
    template<typename Component, typename Pools>
    [[nodiscard]] auto get_component(entity_id const entity, Pools const& pools) {
        using T = std::remove_cvref_t<Component>;

        if constexpr (std::is_pointer_v<T>) {
            static_cast<void>(entity);
            return nullptr;
        } else if constexpr (tagged<T>) {
            static char dummy_arr[sizeof(T)];
            return reinterpret_cast<T*>(dummy_arr);
        } else if constexpr (shared<T> || global<T>) {
            return &get_pool<T>(pools).get_shared_component();
        } else if constexpr (std::is_same_v<reduce_parent_t<T>, parent_id>) {
            using parent_type = std::remove_cvref_t<Component>;
            parent_id pid = *get_pool<parent_id>(pools).find_component_data(entity);
            
            parent_types_tuple_t<parent_type> pt;
            auto const tup_parent_ptrs = std::apply(
                [&](auto... parent_types) { 
                    return std::make_tuple(get_pool<decltype(parent_types)>(pools).find_component_data(pid)...);
            }, pt);

            return parent_type{pid, tup_parent_ptrs};
        } else {
            return get_pool<T>(pools).find_component_data(entity);
        }
    }

    // Extracts a component argument from a tuple
    template<typename Component, typename Tuple>
    decltype(auto) extract_arg(Tuple& tuple, [[maybe_unused]] ptrdiff_t offset) {
        using T = std::remove_cvref_t<Component>;

        if constexpr (std::is_pointer_v<T>) {
            return nullptr;
        } else if constexpr (detail::unbound<T>) {
            T* ptr = std::get<T*>(tuple);
            return *ptr;
        } else if constexpr (detail::is_parent<T>::value) {
            return std::get<T>(tuple);
        } else {
            T* ptr = std::get<T*>(tuple);
            return *(ptr + offset);
        }
    }

    // Gets the type a sorting functions operates on.
    // Has to be outside of system or clang craps itself
    template<class R, class C, class T1, class T2>
    struct get_sort_func_type_impl {
        explicit get_sort_func_type_impl(R (C::*)(T1, T2) const) {
        }

        using type = std::remove_cvref_t<T1>;
    };

    // The type of a single component argument
    template<typename Component>
    using component_argument = std::conditional_t<is_parent<std::remove_cvref_t<Component>>::value,
        std::remove_cvref_t<Component>,   // parent components are stored as copies
        std::remove_cvref_t<Component>*>; // rest are pointers

    // Holds a pointer to the first component from each pool
    template<class FirstComponent, class... Components>
    using argument_tuple = std::conditional_t<is_entity<FirstComponent>,
        std::tuple<component_argument<Components>...>,
        std::tuple<component_argument<FirstComponent>, component_argument<Components>...>>;

    // Tuple holding component pools
    template<class FirstComponent, class... Components>
    using tup_pools = std::conditional_t<is_entity<FirstComponent>,
        std::tuple<pool<reduce_parent_t<Components>>...>,
        std::tuple<pool<reduce_parent_t<FirstComponent>>, pool<reduce_parent_t<Components>>...>>;

} // namespace ecs::detail

#endif // !__SYSTEM_DEFS_H_

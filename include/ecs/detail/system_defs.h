#ifndef __SYSTEM_DEFS_H_
#define __SYSTEM_DEFS_H_

// Contains definitions that are used by the system- and builder classes

namespace ecs::detail {
    // Alias for stored pools
    template<class T>
    using pool = component_pool<std::remove_pointer_t<std::remove_cvref_t<T>>>* const;

    // Get a component pool from a component pool tuple
    template<typename Component, typename Pools>
    component_pool<Component>& get_pool(Pools const& pools) {
        return *std::get<pool<Component>>(pools);
    }

    // Get an entities component from a component pool
    template<typename Component, typename Pools>
    [[nodiscard]] std::remove_cvref_t<Component>* get_component(entity_id const entity, Pools const& pools) {
        using T = std::remove_cvref_t<Component>;

        if constexpr (std::is_pointer_v<T>) {
            static_cast<void>(entity);
            return nullptr;
        } else if constexpr (tagged<T>) {
            static char dummy_arr[sizeof(T)];
            return reinterpret_cast<T*>(dummy_arr);
        } else if constexpr (shared<T> || global<T>) {
            return &get_pool<T>(pools).get_shared_component();
        } else {
            return get_pool<T>(pools).find_component_data(entity);
        }
    }

    // Extracts a component argument from a tuple
    template<typename Component, typename Tuple>
    decltype(auto) extract_arg(Tuple const& tuple, [[maybe_unused]] ptrdiff_t offset) {
        using T = std::remove_cvref_t<Component>;
        if constexpr (std::is_pointer_v<T>) {
            return nullptr;
        } else if constexpr (detail::unbound<T>) {
            T* ptr = std::get<T*>(tuple);
            return *ptr;
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

    // Holds a pointer to the first component from each pool
    template<class FirstComponent, class... Components>
    using argument_tuple =
        std::conditional_t<is_entity<FirstComponent>, std::tuple<std::remove_cvref_t<Components>*...>,
            std::tuple<std::remove_cvref_t<FirstComponent>*, std::remove_cvref_t<Components>*...>>;

    // Tuple holding component pools
    template<class FirstComponent, class... Components>
    using tup_pools = std::conditional_t<is_entity<FirstComponent>, std::tuple<pool<Components>...>,
        std::tuple<pool<FirstComponent>, pool<Components>...>>;

} // namespace ecs::detail

#endif // !__SYSTEM_DEFS_H_

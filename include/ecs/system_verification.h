#ifndef __SYSTEM_VERIFICATION
#define __SYSTEM_VERIFICATION

#include <concepts>
#include <type_traits>

#include "component_specifier.h"

namespace ecs::detail {
    // Given a type T, if it is callable with an entity argument,
    // resolve to the return type of the callable. Otherwise assume the type T.
    template<typename T>
    struct get_type {
        using type = T;
    };

    template<std::invocable<int> T>
    struct get_type<T> {
        using type = std::invoke_result_t<T, int>;
    };

    template<typename T>
    using get_type_t = typename get_type<T>::type;

    // Returns true if all types passed are unique
    template<typename First, typename... T>
    constexpr bool unique_types() {
        if constexpr ((std::is_same_v<First, T> || ...))
            return false;
        else {
            if constexpr (sizeof...(T) == 0)
                return true;
            else
                return unique_types<T...>();
        }
    }

    template<typename First, typename... T>
    constexpr static bool unique_types_v = unique_types<get_type<First>, get_type_t<T>...>();

    // Ensure that any type in the parameter pack T is only present once.
    template<typename First, typename... T>
    concept unique = unique_types_v<First, T...>;

    template<class T>
    concept entity_type =
        std::is_same_v<std::remove_cvref_t<T>, entity_id> || std::is_same_v<std::remove_cvref_t<T>, entity>;

    // Implement the requirements for immutable components
    template<typename C>
    constexpr bool req_immutable() {
        // Components flagged as 'immutable' must also be const
        if constexpr (detail::immutable<C>)
            return std::is_const_v<std::remove_reference_t<C>>;
        else
            return true;
    }

    // Implement the requirements for tagged components
    template<typename C>
    constexpr bool req_tagged() {
        // Components flagged as 'tag' must not be references
        if constexpr (detail::tagged<C>)
            return !std::is_reference_v<C> && (sizeof(C) == 1);
        else
            return true;
    }

    // Implement the requirements for shared components
    template<typename C>
    constexpr bool req_shared() {
        // Components flagged as 'share' must not be tags or global
        if constexpr (detail::shared<C>)
            return !detail::tagged<C> && !detail::global<C>;
        else
            return true;
    }

    // Implement the requirements for global components
    template<typename C>
    constexpr bool req_global() {
        // Components flagged as 'global' must not be tags or shared
        // and must not be marked as 'transient'
        if constexpr (detail::global<C>)
            return !detail::tagged<C> && !detail::shared<C> && !detail::transient<C>;
        else
            return true;
    }

    template<class C>
    concept Component = requires {
        requires(req_immutable<C>() && req_tagged<C>() && req_shared<C>() && req_global<C>());
    };

    template<class R, class FirstArg, class... Args>
    concept checked_system = requires {
        // systems can not return values
        requires std::is_same_v<R, void>;

        // no pointers allowed
        // requires !std::is_pointer_v<FirstArg> && (!std::is_pointer_v<Args> && ...);

        // systems must take at least one component argument
        requires(entity_type<FirstArg> ? sizeof...(Args) >= 1 : true);

        // Make sure the first entity is not passed as a reference
        requires(entity_type<FirstArg> ? !std::is_reference_v<FirstArg> : true);

        // Component types can only be specified once
        requires unique<FirstArg, Args...>;

        // Verify components
        requires Component<FirstArg> && (Component<Args> && ...);
    };

    // A small bridge to allow the Lambda concept to activate the system concept
    template<class R, class C, class... Args>
    requires(sizeof...(Args) > 0 && checked_system<R, Args...>) struct lambda_to_system_bridge {
        lambda_to_system_bridge(R (C::*)(Args...)){};
        lambda_to_system_bridge(R (C::*)(Args...) const){};
        lambda_to_system_bridge(R (C::*)(Args...) noexcept){};
        lambda_to_system_bridge(R (C::*)(Args...) const noexcept){};
    };

    template<typename T>
    concept lambda = requires {
        // Must have the call operator
        &T::operator();

        // Check all the system requirements
        lambda_to_system_bridge(&T::operator());
    };

    template<class R, class T, class U>
    concept checked_sorter = requires {
        // sorter must return boolean
        requires std::is_same_v<R, bool>;

        // Arguments must be of same type
        requires std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

        // Most obey strict ordering
        requires std::totally_ordered_with<T, U>;
    };

    // A small bridge to allow the Lambda concept to activate the sorter concept
    template<class R, class C, class... Args>
    requires(sizeof...(Args) == 2 && checked_sorter<R, Args...>) struct lambda_to_sorter_bridge {
        lambda_to_sorter_bridge(R (C::*)(Args...)){};
        lambda_to_sorter_bridge(R (C::*)(Args...) const){};
        lambda_to_sorter_bridge(R (C::*)(Args...) noexcept){};
        lambda_to_sorter_bridge(R (C::*)(Args...) const noexcept){};
    };

    template<typename T>
    concept sorter = requires {
        // Must have the call operator
        &T::operator();

        // Check all the sorter requirements
        lambda_to_sorter_bridge(&T::operator());
    };
} // namespace ecs::detail

#endif // !__SYSTEM_VERIFICATION

#ifndef __VERIFICATION
#define __VERIFICATION

#include <concepts>
#include <type_traits>

#include "../flags.h"
#include "../entity_id.h"
#include "options.h"

namespace ecs::detail {
    // Given a type T, if it is callable with an entity argument,
    // resolve to the return type of the callable. Otherwise assume the type T.
    template<typename T>
    struct get_type {
        using type = T;
    };

    template<std::invocable<entity_type> T>
    struct get_type<T> {
        using type = std::invoke_result_t<T, entity_type>;
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
    constexpr static bool is_entity = std::is_same_v<std::remove_cvref_t<T>, entity_id>;

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

    // Implement the requirements for global components
    template<typename C>
    constexpr bool req_global() {
        // Components flagged as 'global' must not be tags or shared
        // and must not be marked as 'transient'
        if constexpr (detail::global<C>)
            return !detail::tagged<C> && !detail::transient<C>;
        else
            return true;
    }

    // Implement the requirements for ecs::parent components
    template<typename C>
    constexpr bool req_parent() {
        // Parent components must always be passed as references
        /*if constexpr (detail::is_parent<C>::value) {
            return std::is_reference_v<C>;
        }
        else*/
            return true;
    }


    template<class C>
    concept Component = req_parent<C>() && req_immutable<C>() && req_tagged<C>() && req_global<C>();


    template<class R, class FirstArg, class... Args>
    concept checked_system = 
            // systems can not return values
            std::is_same_v<R, void> &&

            // systems must take at least one component argument
            (is_entity<FirstArg> ? (sizeof...(Args)) > 0 : true) &&

            // Make sure the first entity is not passed as a reference
            (is_entity<FirstArg> ? !std::is_reference_v<FirstArg> : true) &&

            // Component types can only be specified once
            // requires unique<FirstArg, Args...>; // ICE's gcc 10.1
            unique_types_v<FirstArg, Args...> &&

            // Verify components
            (Component<FirstArg> && (Component<Args> && ...));

    // A small bridge to allow the Lambda concept to activate the system concept
    template<class R, class C, class FirstArg, class... Args>
    requires(checked_system<R, FirstArg, Args...>)
    struct lambda_to_system_bridge {
        lambda_to_system_bridge(R (C::*)(FirstArg, Args...)) {};
        lambda_to_system_bridge(R (C::*)(FirstArg, Args...) const) {};
        lambda_to_system_bridge(R (C::*)(FirstArg, Args...) noexcept) {};
        lambda_to_system_bridge(R (C::*)(FirstArg, Args...) const noexcept) {};
    };

    template<typename T>
    concept lambda = requires {
        // Check all the system requirements
        lambda_to_system_bridge(&T::operator());
    };

    template<class R, class T, class U>
    concept checked_sorter =
        // sorter must return boolean
        std::is_same_v<R, bool> &&

        // Arguments must be of same type
        std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

    // A small bridge to allow the Lambda concept to activate the sorter concept
    template<class R, class C, class T, class U>
    requires(checked_sorter<R, T, U>) struct lambda_to_sorter_bridge {
        lambda_to_sorter_bridge(R (C::*)(T, U)){};
        lambda_to_sorter_bridge(R (C::*)(T, U) const){};
        lambda_to_sorter_bridge(R (C::*)(T, U) noexcept){};
        lambda_to_sorter_bridge(R (C::*)(T, U) const noexcept){};
    };

    template<typename T>
    concept sorter = requires {
        // Check all the sorter requirements
        lambda_to_sorter_bridge(&T::operator());
    };
} // namespace ecs::detail

#endif // !__VERIFICATION

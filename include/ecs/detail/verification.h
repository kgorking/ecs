#ifndef __VERIFICATION
#define __VERIFICATION

#include <concepts>
#include <type_traits>

#include "../flags.h"
#include "../entity_id.h"
#include "options.h"
#include "system_defs.h"

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

    // Gets the type a sorting function operates on.
    template<class R, class A, class B, class ...C>
    struct get_sorter_types_impl {
        explicit get_sorter_types_impl(R (*)(A, B)) {
            static_assert(sizeof...(C) == 0, "two arguments expected in sorting predicate");
            static_assert(std::is_same_v<A, B>, "types must be identical");
        }
        explicit get_sorter_types_impl(R (A::*)(B, C...) const) {
            static_assert(sizeof...(C) == 1, "two arguments expected in sorting predicate");
            static_assert(std::is_same_v<B, C...>, "types must be identical");
        }

        using type1 = std::conditional_t<sizeof...(C)==0,
            std::remove_cvref_t<A>,
            std::remove_cvref_t<B>>;
        using type2 = std::conditional_t<sizeof...(C)==0,
            std::remove_cvref_t<B>,
            std::remove_cvref_t<std::tuple_element_t<0, std::tuple<C...>>>>;
    };

    template<class T1, class T2>
    struct get_sorter_types {
        template<class T> requires requires { &T::operator(); }
        get_sorter_types(T);

        template<class R, class A, class B>
        get_sorter_types(R (*)(A, B));

        using type1 = std::remove_cvref_t<T1>;
        using type2 = std::remove_cvref_t<T2>;
    };

    template<class R, class A, class B>
    get_sorter_types(R (*)(A, B)) -> get_sorter_types<A, B>;

    template<class T>
    get_sorter_types(T) -> get_sorter_types< // get_sorter_types(&T::operator()) // deduction guides can't refer to other guides :/
        typename decltype(get_sorter_types_impl(&T::operator()))::type1,
        typename decltype(get_sorter_types_impl(&T::operator()))::type2>;


    // Implement the requirements for ecs::parent components
    template<typename C>
    constexpr void verify_parent_component() {
        if constexpr (detail::is_parent<std::remove_cvref_t<C>>::value) {
            // If there is one-or-more sub-components,
            // then the parent must be passed as a reference
            constexpr size_t num_parent_subtype_filters =
                count_ptrs_in_tuple<0, parent_types_tuple_t<std::remove_cvref_t<C>>>();
            constexpr size_t num_parent_subtypes =
                std::tuple_size_v<parent_types_tuple_t<std::remove_cvref_t<C>>> - num_parent_subtype_filters;

            if constexpr (num_parent_subtypes > 0) {
                static_assert(
                    std::is_reference_v<C>, "parents with non-filter sub-components must be passed as references");
            }
        }
    }
    
    // Implement the requirements for tagged components
    template<typename C>
    constexpr void verify_tagged_component() {
        if constexpr (detail::tagged<C>)
            static_assert(!std::is_reference_v<C> && (sizeof(C) == 1), "components flagged as 'tag' must not be references");
    }

    // Implement the requirements for global components
    template<typename C>
    constexpr void verify_global_component() {
        if constexpr (detail::global<C>)
            static_assert(!detail::tagged<C> && !detail::transient<C>, "components flagged as 'global' must not be 'tag's or 'transient'");
    }

    // Implement the requirements for immutable components
    template<typename C>
    constexpr void verify_immutable_component() {
        if constexpr (detail::immutable<C>)
            static_assert(std::is_const_v<std::remove_reference_t<C>>, "components flagged as 'immutable' must also be const");
    }

    template<class R, class FirstArg, class... Args>
    constexpr void system_verifier() {
        static_assert(std::is_same_v<R, void>, "systems can not have returnvalues");

        static_assert(unique_types_v<FirstArg, Args...>, "component parameter types can only be specified once");

        if constexpr (is_entity<FirstArg>) {
            static_assert(sizeof...(Args) > 0, "systems must take at least one component argument");

            // Make sure the first entity is not passed as a reference
            static_assert(!std::is_reference_v<FirstArg>, "ecs::entity_id must not be passed as a reference");
        }

        verify_immutable_component<FirstArg>();
        (verify_immutable_component<Args>(), ...);

        verify_global_component<FirstArg>();
        (verify_global_component<Args>(), ...);

        verify_tagged_component<FirstArg>();
        (verify_tagged_component<Args>(), ...);

        verify_parent_component<FirstArg>();
        (verify_parent_component<Args>(), ...);
    }

    // A small bridge to allow the Lambda to activate the system verifier
    template<class R, class C, class FirstArg, class... Args>
    struct system_to_lambda_bridge {
        system_to_lambda_bridge(R (C::*)(FirstArg, Args...)) {
            system_verifier<R, FirstArg, Args...>();
        };
        system_to_lambda_bridge(R (C::*)(FirstArg, Args...) const){
            system_verifier<R, FirstArg, Args...>();
        };
        system_to_lambda_bridge(R (C::*)(FirstArg, Args...) noexcept){
            system_verifier<R, FirstArg, Args...>();
        };
        system_to_lambda_bridge(R (C::*)(FirstArg, Args...) const noexcept){
            system_verifier<R, FirstArg, Args...>();
        };
    };

    // A small bridge to allow the function to activate the system verifier
    template<class R, class FirstArg, class... Args>
    struct system_to_func_bridge {
        system_to_func_bridge(R (*)(FirstArg, Args...)) {
            system_verifier<R, FirstArg, Args...>();
        };
        system_to_func_bridge(R (*)(FirstArg, Args...) noexcept) {
            system_verifier<R, FirstArg, Args...>();
        };
    };


    template<typename T>
    concept type_is_lambda = requires {
        &T::operator();
    };

    template<typename T>
    concept type_is_function = requires (T t) {
        system_to_func_bridge{t};
    };

    template<typename TupleOptions, typename SystemFunc, typename SortFunc>
    void make_system_parameter_verifier() {
        bool constexpr is_lambda = type_is_lambda<SystemFunc>;
        bool constexpr is_func = type_is_function<SystemFunc>;

        static_assert(is_lambda || is_func, "Systems can only be created from lambdas or free-standing functions");

        // verify the system function
        if constexpr (is_lambda) {
            system_to_lambda_bridge const stlb(&SystemFunc::operator());
        } else if constexpr(is_func) {
            system_to_func_bridge const stfb(SystemFunc{});
        }

        // verify the sort function
        if constexpr (!std::is_same_v<std::nullptr_t, SortFunc>) {
            bool constexpr is_sort_lambda = type_is_lambda<SystemFunc>;
            bool constexpr is_sort_func = type_is_function<SystemFunc>;

            static_assert(is_sort_lambda || is_sort_func, "invalid sorting function");

            using sort_types = decltype(get_sorter_types(SortFunc{}));
            if constexpr (is_sort_lambda) {
                static_assert(std::predicate<SortFunc, typename sort_types::type1, typename sort_types::type2>,
                    "Sorting function is not a predicate");
            } else if constexpr (is_sort_func) {
                static_assert(std::predicate<SortFunc, typename sort_types::type1, typename sort_types::type2>,
                    "Sorting function is not a predicate");
            }
        }
    }

} // namespace ecs::detail

#endif // !__VERIFICATION

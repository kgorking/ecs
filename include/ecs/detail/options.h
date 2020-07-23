#pragma once
#include <execution>

namespace ecs::detail {
    //
    // Check if type is a group
    template<typename T>
    struct is_group {
        static constexpr bool value = false;
    };
    template<typename T>
    requires requires {
        T::group_id;
    }
    struct is_group<T> {
        static constexpr bool value = true;
    };

    //
    // Check if the options has an execution policy

    // Contains detectors for the options


    // The main detector. Takes a tester structure and a tuple of options to test against.
    // The tester must have static member 'value' that determines if the option passed to it
    // is what it is looking for, see 'is_group' below for an example.
    // STL testers like 'std::is_execution_policy' can also be used
    template<template<class O> class Tester, class TupleOptions, class NotFoundType = void>
    constexpr auto get_option() {
        auto constexpr option_index_finder = [](auto... options) -> int {
            int index = -1;
            int counter = 0;

            auto x = [&](auto opt) {
                if (index == -1 && Tester<decltype(opt)>::value)
                    index = counter;
                else
                    counter += 1;
            };

            (..., x(options));

            return index;
        };

        constexpr int option_index = std::apply(option_index_finder, TupleOptions{});
        if constexpr (option_index != -1) {
            using opt_type = std::tuple_element_t<option_index, TupleOptions>;
            return (opt_type*) 0;
        } else {
            return (NotFoundType*) 0;
        }
    }

    template<class T>
    constexpr bool is_valid_option(T) {
        return !std::is_void_v<std::remove_pointer_t<T>>;
    }

    template<template<class O> class Tester, class TupleOptions>
    using get_option_type = std::remove_pointer_t<decltype(get_option<Tester, TupleOptions>())>;

    template<template<class O> class Tester, class TupleOptions, class NotFoundType>
    using get_option_type_or =
        std::remove_pointer_t<decltype(get_option<Tester, TupleOptions, NotFoundType>())>;

    template<class Opt>
    static constexpr bool is_valid_option_type = !std::is_same_v<void*, Opt>;

    template<template<class O> class Tester, class TupleOptions>
    constexpr bool has_option() {
        return is_valid_option(get_option<Tester, TupleOptions>());
    }
} // namespace ecs::detail

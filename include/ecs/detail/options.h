#ifndef __DETAIL_OPTIONS_H
#define __DETAIL_OPTIONS_H

#include <execution>

namespace ecs::detail {
    //
    // Check if type is a group
    template<typename T>
    struct is_group {
        static constexpr bool value = false;
    };
    template<typename T>
    requires requires { T::group_id; }
    struct is_group<T> {
        static constexpr bool value = true;
    };

    //
    // Check if type is a frequency
    template<typename T>
    struct is_frequency {
        static constexpr bool value = false;
    };
    template<typename T>
    requires requires { T::hz; }
    struct is_frequency<T> {
        static constexpr bool value = true;
    };

    // Contains detectors for the options
    namespace detect {
        // A detector that applies Tester to each option.
        template<template<class O> class Tester, class TupleOptions, class NotFoundType = void>
        constexpr auto test_option() {
            if constexpr (std::tuple_size_v<TupleOptions> == 0) {
                return (NotFoundType*) 0;
            } else {
                auto constexpr option_index_finder = [](auto... options) -> int {
                    int index = -1;

                    (..., [&, counter = 0](auto opt) mutable {
                        if (index == -1 && Tester<decltype(opt)>::value)
                            index = counter;
                        else
                            counter += 1;
                    }(options));

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
        }

        template<class Option, class TupleOptions>
        constexpr bool has_option() {
            if constexpr (std::tuple_size_v<TupleOptions> == 0) {
                return false;
            } else {
                auto constexpr option_index_finder = [](auto... options) -> int {
                    int index = -1;
                    int counter = 0;

                    auto x = [&](auto opt) {
                        if (index == -1 && std::is_same_v<Option, decltype(opt)>)
                            index = counter;
                        else
                            counter += 1;
                    };

                    (..., x(options));

                    return index;
                };

                constexpr int option_index = std::apply(option_index_finder, TupleOptions{});
                return option_index != -1;
            }
        }
    } // namespace detect

    // Use a tester to check the options. Takes a tester structure and a tuple of options to test against.
    // The tester must have static member 'value' that determines if the option passed to it
    // is what it is looking for, see 'is_group' for an example.
    // STL testers like 'std::is_execution_policy' can also be used
    template<template<class O> class Tester, class TupleOptions>
    using test_option_type = std::remove_pointer_t<decltype(detect::test_option<Tester, TupleOptions>())>;

    // Use a tester to check the options. Results in 'NotFoundType' if the tester
    // does not find a viable option.
    template<template<class O> class Tester, class TupleOptions, class NotFoundType>
    using test_option_type_or =
        std::remove_pointer_t<decltype(detect::test_option<Tester, TupleOptions, NotFoundType>())>;

    template<class Option, class TupleOptions>
    constexpr bool has_option() {
        return detect::has_option<Option, TupleOptions>();
    }
} // namespace ecs::detail

#endif // !__DETAIL_OPTIONS_H

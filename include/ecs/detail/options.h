#ifndef ECS_DETAIL_OPTIONS_H
#define ECS_DETAIL_OPTIONS_H

#include "../options.h"
#include "type_list.h"

namespace ecs::detail {

//
// Check if type is a group
template <typename T>
struct is_group {
	static constexpr bool value = false;
};
template <typename T>
requires requires {
	T::group_id;
}
struct is_group<T> {
	static constexpr bool value = true;
};

//
// Check if type is an interval
template <typename T>
struct is_interval {
	static constexpr bool value = false;
};
template <typename T>
requires requires {
	T::_ecs_duration;
}
struct is_interval<T> {
	static constexpr bool value = true;
};

//
// Check if type is a parent
template <typename T>
struct is_parent {
	static constexpr bool value = false;
};
template <typename T>
requires requires {
	typename T::_ecs_parent;
}
struct is_parent<T> {
	static constexpr bool value = true;
};

// Contains detectors for the options
namespace detect {
template <int Index, template <class O> class Tester, class ListOptions>
constexpr int find_tester_index() {
	if constexpr (Index == type_list_size<ListOptions>) {
		return -1; // type not found
	} else if constexpr (Tester<type_list_at<Index, ListOptions>>::value) {
		return Index;
	} else {
		return find_tester_index<Index + 1, Tester, ListOptions>();
	}
}

template <int Index, typename Option, class ListOptions>
constexpr int find_type_index() {
	if constexpr (Index == type_list_size<ListOptions>) {
		return -1; // type not found
	} else if constexpr (std::is_same_v<Option, type_list_at<Index, ListOptions>>) {
		return Index;
	} else {
		return find_type_index<Index + 1, Option, ListOptions>();
	}
}

// A detector that applies Tester to each option.
template <template <class O> class Tester, class ListOptions, class NotFoundType = void>
constexpr auto test_option() {
	if constexpr (type_list_size<ListOptions> == 0) {
		return (NotFoundType*)0;
	} else {
		constexpr int option_index = find_tester_index<0, Tester, ListOptions>();
		if constexpr (option_index != -1) {
			using opt_type = type_list_at<option_index, ListOptions>;
			return (opt_type*)0;
		} else {
			return (NotFoundType*)0;
		}
	}
}

template <class Option, class ListOptions>
constexpr bool has_option() {
	if constexpr (type_list_size<ListOptions> == 0) {
		return false;
	} else {
		constexpr int option_index = find_type_index<0, Option, ListOptions>();
		return option_index != -1;
	}
}
} // namespace detect

// Use a tester to check the options. Takes a tester structure and a tuple of options to test against.
// The tester must have static member 'value' that determines if the option passed to it
// is what it is looking for, see 'is_group' for an example.
// STL testers like 'std::is_execution_policy' can also be used
template <template <class O> class Tester, class TupleOptions>
using test_option_type = std::remove_pointer_t<decltype(detect::test_option<Tester, TupleOptions>())>;

// Use a tester to check the options. Results in 'NotFoundType' if the tester
// does not find a viable option.
template <template <class O> class Tester, class TupleOptions, class NotFoundType>
using test_option_type_or = std::remove_pointer_t<decltype(detect::test_option<Tester, TupleOptions, NotFoundType>())>;

// Use a tester to find the index of a type in the tuple. Results in -1 if not found
template <template <class O> class Tester, class TupleOptions>
static constexpr int test_option_index = detect::find_tester_index<0, Tester, TupleOptions>();

template <class Option, class TupleOptions>
constexpr bool has_option() {
	return detect::has_option<Option, TupleOptions>();
}

} // namespace ecs::detail

#endif // !ECS_DETAIL_OPTIONS_H

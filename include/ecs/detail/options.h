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
	T::ms;
	T::us;
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
requires requires { typename std::remove_cvref_t<T>::_ecs_parent; }
struct is_parent<T> {
	static constexpr bool value = true;
};



// Contains detectors for the options
namespace detect {
	template <template <typename O> typename Tester, typename ListOptions>
	constexpr int find_tester_index() {
		int found = 0;
		int index = 0;
		for_each_type<ListOptions>([&]<typename OptionFromList>() {
			found = !found && Tester<OptionFromList>::value;
			index += !found;
		});

		if (index == type_list_size<ListOptions>)
			return -1;
		return index;
	}

	template <typename Option, typename ListOptions>
	constexpr int find_type_index() {
		int found = 0;
		int index = 0;
		for_each_type<ListOptions>([&]<typename OptionFromList>() {
			found = !found && std::is_same_v<Option, OptionFromList>;
			index += !found;
		});

		if (index == type_list_size<ListOptions>)
			return -1;
		return index;
	}

	// A detector that applies Tester to each option.
	template <template <typename O> typename Tester, typename ListOptions, typename NotFoundType = void>
	constexpr auto test_option() {
		auto const lambda = []<typename T>() {
			return static_cast<std::remove_cvref_t<T>*>(nullptr);
		};
		using Type = decltype(run_if<Tester, ListOptions>(lambda));

		if constexpr (!std::is_same_v<Type, void>) {
			return Type{};
		} else {
			return static_cast<NotFoundType*>(nullptr);
		}
	}

	template <typename Type>
	struct type_detector {
		template<typename... Types>
		consteval static bool test(type_list<Types...>*) noexcept {
			return (test(static_cast<Types*>(nullptr)) || ...);
		}

		consteval static bool test(Type*) noexcept {
			return true;
		}
		consteval static bool test(...) noexcept {
			return false;
		}
	};

} // namespace detect

// Use a tester to check the options. Takes a tester structure and a type_list of options to test against.
// The tester must have static member 'value' that determines if the option passed to it
// is what it is looking for, see 'is_group' for an example.
// STL testers like 'std::is_execution_policy' can also be used
template <template <typename O> typename Tester, typename TypelistOptions>
using test_option_type = std::remove_pointer_t<decltype(detect::test_option<Tester, TypelistOptions>())>;

// Use a tester to check the options. Results in 'NotFoundType' if the tester
// does not find a viable option.
template <template <typename O> typename Tester, typename TypelistOptions, typename NotFoundType>
using test_option_type_or = std::remove_pointer_t<decltype(detect::test_option<Tester, TypelistOptions, NotFoundType>())>;

// Use a tester to find the index of a type in the tuple. Results in -1 if not found
template <template <typename O> typename Tester, typename TypelistOptions>
static constexpr int test_option_index = detect::find_tester_index<Tester, TypelistOptions>();

template <typename Option, typename ListOptions>
constexpr bool has_option() {
	return detect::type_detector<Option>::test(static_cast<ListOptions*>(nullptr));
}

} // namespace ecs::detail

#endif // !ECS_DETAIL_OPTIONS_H

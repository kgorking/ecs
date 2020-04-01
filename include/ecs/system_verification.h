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

	template<typename T> requires std::invocable<T, int>
	struct get_type<T> {
		using type = std::invoke_result_t<T, int>;
	};

	template<typename T>
	using get_type_t = typename get_type<T>::type;

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

	template<typename... T>
	constexpr static bool unique_types_v = unique_types<get_type_t<T>...>();

	// Ensure that any type in the parameter pack T is only present once.
	template<typename... T>
	concept unique = unique_types_v<T...>;

	template <class T>
	concept entity_type = std::is_same_v<std::remove_cvref_t<T>, entity_id> || std::is_same_v<std::remove_cvref_t<T>, entity>;

	template <class R, class FirstArg, class ...Args>
	concept checked_system = requires {
		// systems can not return values
		requires std::is_same_v<R, void>;

		// no pointers allowed
		requires !std::is_pointer_v<FirstArg> && (!std::is_pointer_v<Args>&& ...);

		// systems must take at least one component argument
		requires (entity_type<FirstArg> ? sizeof...(Args) >= 1 : true);

		// Make sure the first entity is not passed as a reference
		requires (entity_type<FirstArg> ? !std::is_reference_v<FirstArg> : true);

		// Component types can only be specified once
		requires unique<FirstArg, Args...>;

		// Components flagged as 'immutable' must also be const
		requires
			(detail::immutable<FirstArg> ? std::is_const_v<std::remove_reference_t<FirstArg>> : true) &&
			((detail::immutable<Args> ? std::is_const_v<std::remove_reference_t<Args>> : true) && ...);

		// Components flagged as 'tag' must not be references
		requires
			(detail::tagged<FirstArg> ? !std::is_reference_v<FirstArg> : true) &&
			((detail::tagged<Args> ? !std::is_reference_v<Args> : true) && ...);

		// Components flagged as 'tag' must not hold data
		requires
			(detail::tagged<FirstArg> ? sizeof(FirstArg) == 1 : true) &&
			((detail::tagged<Args> ? sizeof(Args) == 1 : true) && ...);

		// Components flagged as 'share' must not be 'tag'ged
		requires
			(detail::shared<FirstArg> ? !detail::tagged<FirstArg> : true) &&
			((detail::shared<Args> ? !detail::tagged<Args> : true) && ...);
	};

	// A small bridge to allow the Lambda concept to activate the system concept
	template <class R, class C, class FirstArg, class ...Args>
		requires (checked_system<R, FirstArg, Args...>)
	struct lambda_to_system_bridge {
		lambda_to_system_bridge(R(C::*)(FirstArg, Args...)) {};
		lambda_to_system_bridge(R(C::*)(FirstArg, Args...) const) {};
		lambda_to_system_bridge(R(C::*)(FirstArg, Args...) noexcept) {};
		lambda_to_system_bridge(R(C::*)(FirstArg, Args...) const noexcept) {};
	};

	template <typename T>
	concept lambda = requires {
		// Must have the call operator
		&T::operator ();

		// Check all the system requirements
		//lambda_to_system_bridge(&T::operator ());
	};
}

#endif // !__SYSTEM_VERIFICATION

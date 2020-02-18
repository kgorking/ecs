#pragma once
#include <type_traits>

namespace ecs
{
	// Add this in 'ecs_flags()' to mark a component as a tag.
	// Uses O(1) memory instead of O(n).
	struct tag {};

	// Add this in 'ecs_flags()' to mark a component as shared between components,
	// meaning that any entity with a shared component will all point to the same component.
	// Think of it as a static member variable in a regular class.
	// Uses O(1) memory instead of O(n).
	struct shared {};

	// Add this in 'ecs_flags()' to mark a component as transient.
	// The component will only exist on an entity for one cycle,
	// and then be automatically removed.
	struct transient {};

	// Add this in 'ecs_flags()' to mark a component as unmutable.
	// A compile-time error will be raised if a system tries to
	// access the component through a non-const reference.
	struct unmutable {};

	// Add flags to a component to change its behaviour and memory usage.
	// Example:
	// struct my_component {
	// 	ecs_flags(ecs::tag, ecs::transient);
	// 	// component data
	// };
	#define ecs_flags(...) struct _ecs_flags : __VA_ARGS__ {};

	namespace detail {
		template <typename T> constexpr auto is_tagged(...)    -> bool { return false; }
		template <typename T> constexpr auto is_shared(...)    -> bool { return false; }
		template <typename T> constexpr auto is_transient(...) -> bool { return false; }
		template <typename T> constexpr auto is_unmutable(...) -> bool { return false; }

		template <typename T> constexpr auto is_tagged(int)    -> decltype(typename T::_ecs_flags{}, bool{}) { return std::is_base_of_v<ecs::tag, typename T::_ecs_flags>; }
		template <typename T> constexpr auto is_shared(int)    -> decltype(typename T::_ecs_flags{}, bool{}) { return std::is_base_of_v<ecs::shared, typename T::_ecs_flags>; }
		template <typename T> constexpr auto is_transient(int) -> decltype(typename T::_ecs_flags{}, bool{}) { return std::is_base_of_v<ecs::transient, typename T::_ecs_flags>; }
		template <typename T> constexpr auto is_unmutable(int) -> decltype(typename T::_ecs_flags{}, bool{}) { return std::is_base_of_v<ecs::unmutable, typename T::_ecs_flags>; }

		// Some helpers
		template <class T> constexpr static bool is_tagged_v = is_tagged<T>(0);
		template <class T> constexpr static bool is_shared_v = is_shared<T>(0);
		template <class T> constexpr static bool is_transient_v = is_transient<T>(0);
		template <class T> constexpr static bool is_unmutable_v = is_unmutable<T>(0);

		template <class T> constexpr static bool is_static_v = (is_shared_v<T> || is_tagged_v<T>); // all entities point to the same component
	}
}

#ifndef __SYSTEM_DEFS_H_
#define __SYSTEM_DEFS_H_

// Contains definitions that are used by the system- and builder classes
#include "component_pool.h"
#include "parent_id.h"

namespace ecs::detail {
template <class T>
constexpr static bool is_entity = std::is_same_v<std::remove_cvref_t<T>, entity_id>;

// If given a parent, convert to detail::parent_id, otherwise do nothing
template <typename T>
using reduce_parent_t =
	std::conditional_t<std::is_pointer_v<T>,
		std::conditional_t<is_parent<std::remove_pointer_t<T>>::value, parent_id *, T>,
		std::conditional_t<is_parent<T>::value, parent_id, T>>;

// Alias for stored pools
template <class T>
using pool = component_pool<std::remove_pointer_t<std::remove_cvref_t<reduce_parent_t<T>>>> *const;

// Helper to extract the parent types
template <typename T>
struct parent_type_detect; // primary template

template <template <class...> class Parent, class... ParentComponents> // partial specialization
struct parent_type_detect<Parent<ParentComponents...>> {
	static_assert(!(is_parent<ParentComponents>::value || ...), "parents in parents not supported");
	using type = std::tuple<ParentComponents...>;
};

// Helper to extract the parent pool types
template <typename T>
struct parent_pool_detect; // primary template

template <template <class...> class Parent, class... ParentComponents> // partial specialization
struct parent_pool_detect<Parent<ParentComponents...>> {
	static_assert(!(is_parent<ParentComponents>::value || ...), "parents in parents not supported");
	using type = std::tuple<pool<ParentComponents>...>;
};

template <typename T>
using parent_types_tuple_t = typename parent_type_detect<T>::type;
template <typename T>
using parent_pool_tuple_t = typename parent_pool_detect<T>::type;

template <int Index, class Tuple>
constexpr int count_ptrs_in_tuple() {
	if constexpr (Index == std::tuple_size_v<Tuple>) {
		return 0;
	} else if constexpr (std::is_pointer_v<std::tuple_element_t<Index, Tuple>>) {
		return 1 + count_ptrs_in_tuple<Index + 1, Tuple>();
	} else {
		return count_ptrs_in_tuple<Index + 1, Tuple>();
	}
}

// Get a component pool from a component pool tuple.
// Removes cvref and pointer from Component
template <typename Component, typename Pools>
auto &get_pool(Pools const &pools) {
	using T = std::remove_pointer_t<std::remove_cvref_t<reduce_parent_t<Component>>>;
	return *std::get<pool<T>>(pools);
}

// Get a pointer to an entities component data from a component pool tuple.
// If the component type is a pointer, return nullptr
template <typename Component, typename Pools>
Component *get_entity_data([[maybe_unused]] entity_id id, [[maybe_unused]] Pools const &pools) {
	// If the component type is a pointer, return a nullptr
	if constexpr (std::is_pointer_v<Component>) {
		return nullptr;
	} else {
		component_pool<Component> &pool = get_pool<Component>(pools);
		return pool.find_component_data(id);
	}
}

// Get an entities component from a component pool
template <typename Component, typename Pools>
[[nodiscard]] auto get_component(entity_id const entity, Pools const &pools) {
	using T = std::remove_cvref_t<Component>;

	// Filter: return a nullptr
	if constexpr (std::is_pointer_v<T>) {
		static_cast<void>(entity);
		return nullptr;

		// Tag: return a pointer to some dummy storage
	} else if constexpr (tagged<T>) {
		static char dummy_arr[sizeof(T)];
		return reinterpret_cast<T *>(dummy_arr);

		// Global: return the shared component
	} else if constexpr (global<T>) {
		return &get_pool<T>(pools).get_shared_component();

		// Parent component: return the parent with the types filled out
	} else if constexpr (std::is_same_v<reduce_parent_t<T>, parent_id>) {
		using parent_type = std::remove_cvref_t<Component>;
		parent_id pid = *get_pool<parent_id>(pools).find_component_data(entity);

		parent_types_tuple_t<parent_type> pt;
		auto const tup_parent_ptrs =
			std::apply([&](auto... parent_types) { return std::make_tuple(get_entity_data<decltype(parent_types)>(pid, pools)...); }, pt);

		return parent_type{pid, tup_parent_ptrs};

		// Standard: return the component from the pool
	} else {
		return get_pool<T>(pools).find_component_data(entity);
	}
}

// Extracts a component argument from a tuple
template <typename Component, typename Tuple>
decltype(auto) extract_arg(Tuple &tuple, [[maybe_unused]] ptrdiff_t offset) {
	using T = std::remove_cvref_t<Component>;

	if constexpr (std::is_pointer_v<T>) {
		return nullptr;
	} else if constexpr (detail::unbound<T>) {
		T *ptr = std::get<T *>(tuple);
		return *ptr;
	} else if constexpr (detail::is_parent<T>::value) {
		return std::get<T>(tuple);
	} else {
		T *ptr = std::get<T *>(tuple);
		return *(ptr + offset);
	}
}

// The type of a single component argument
template <typename Component>
using component_argument = std::conditional_t<is_parent<std::remove_cvref_t<Component>>::value,
											  std::remove_cvref_t<Component>,		// parent components are stored as copies
											  std::remove_cvref_t<Component> *>;	// rest are pointers

// Holds a pointer to the first component from each pool
template <class FirstComponent, class... Components>
using argument_tuple = std::conditional_t<is_entity<FirstComponent>,
	std::tuple<component_argument<Components>...>,
	std::tuple<component_argument<FirstComponent>, component_argument<Components>...>>;

// Holds a single entity id and its arguments
template<class FirstComponent, class... Components>
using single_argument = decltype(std::tuple_cat(std::tuple<entity_id>{0}, std::declval<argument_tuple<FirstComponent, Components...>>()));

// Holds an entity range and its arguments
template<class FirstComponent, class... Components>
using range_argument = decltype(std::tuple_cat(std::tuple<entity_range>{{0, 1}}, std::declval<argument_tuple<FirstComponent, Components...>>()));

// Tuple holding component pools
template <class FirstComponent, class... Components>
using tup_pools = std::conditional_t<is_entity<FirstComponent>, std::tuple<pool<reduce_parent_t<Components>>...>,
									 std::tuple<pool<reduce_parent_t<FirstComponent>>, pool<reduce_parent_t<Components>>...>>;

} // namespace ecs::detail

#endif // !__SYSTEM_DEFS_H_

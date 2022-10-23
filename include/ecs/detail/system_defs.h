#ifndef ECS_SYSTEM_DEFS_H_
#define ECS_SYSTEM_DEFS_H_

// Contains definitions that are used by the systems classes
#include "component_pool.h"
#include "parent_id.h"
#include "type_list.h"

namespace ecs::detail {
template <typename T>
constexpr static bool is_entity = std::is_same_v<std::remove_cvref_t<T>, entity_id>;

// If given a parent, convert to detail::parent_id, otherwise do nothing
template <typename T>
using reduce_parent_t =
	std::conditional_t<std::is_pointer_v<T>, std::conditional_t<is_parent<std::remove_pointer_t<T>>::value, parent_id*, T>,
					   std::conditional_t<is_parent<T>::value, parent_id, T>>;

// Alias for stored pools
template <typename T>
using pool = component_pool<std::remove_pointer_t<std::remove_cvref_t<reduce_parent_t<T>>>>* const;

// Returns true if a type is read-only
template <typename T>
constexpr bool is_read_only() {
	return detail::immutable<T> || detail::tagged<T> || std::is_const_v<std::remove_reference_t<T>>;
}

// Helper to extract the parent types
template <typename T>
struct parent_type_list; // primary template

template <>
struct parent_type_list<void> {
	using type = void;
}; // partial specialization for void

template <template <typename...> typename Parent, typename... ParentComponents> // partial specialization
struct parent_type_list<Parent<ParentComponents...>> {
	static_assert(!(is_parent<ParentComponents>::value || ...), "parents in parents not supported");
	using type = type_list<ParentComponents...>;
};
template <typename T>
using parent_type_list_t = typename parent_type_list<std::remove_cvref_t<T>>::type;

// Helper to extract the parent pool types
template <typename T>
struct parent_pool_detect; // primary template

template <template <typename...> typename Parent, typename... ParentComponents> // partial specialization
struct parent_pool_detect<Parent<ParentComponents...>> {
	static_assert(!(is_parent<ParentComponents>::value || ...), "parents in parents not supported");
	using type = std::tuple<pool<ParentComponents>...>;
};
template <typename T>
using parent_pool_tuple_t = typename parent_pool_detect<T>::type;

// Get a component pool from a component pool tuple.
// Removes cvref and pointer from Component
template <typename Component, typename Pools>
auto& get_pool(Pools const& pools) {
	using T = std::remove_pointer_t<std::remove_cvref_t<reduce_parent_t<Component>>>;
	return pools.template get<T>();
}

// Get a pointer to an entities component data from a component pool tuple.
// If the component type is a pointer, return nullptr
template <typename Component, typename Pools>
Component* get_entity_data([[maybe_unused]] entity_id id, [[maybe_unused]] Pools const& pools) {
	// If the component type is a pointer, return a nullptr
	if constexpr (std::is_pointer_v<Component>) {
		return nullptr;
	} else {
		component_pool<Component>& pool = get_pool<Component>(pools);
		return pool.find_component_data(id);
	}
}

// Get an entities component from a component pool
template <typename Component, typename Pools>
[[nodiscard]] auto get_component(entity_id const entity, Pools const& pools) {
	using T = std::remove_cvref_t<Component>;

	if constexpr (std::is_pointer_v<T>) {
		// Filter: return a nullptr
		static_cast<void>(entity);
		return static_cast<T*>(nullptr);

	} else if constexpr (tagged<T>) {
		// Tag: return a pointer to some dummy storage
		thread_local char dummy_arr[sizeof(T)];
		return reinterpret_cast<T*>(dummy_arr);

	} else if constexpr (global<T>) {
		// Global: return the shared component
		return &get_pool<T>(pools).get_shared_component();

	} else if constexpr (std::is_same_v<reduce_parent_t<T>, parent_id>) {
		return get_pool<parent_id>(pools).find_component_data(entity);

		// Parent component: return the parent with the types filled out
		//parent_id pid = *get_pool<parent_id>(pools).find_component_data(entity);

		// using parent_type = std::remove_cvref_t<Component>;
		// auto const tup_parent_ptrs = apply_type<parent_type_list_t<parent_type>>(
		//	[&]<typename... ParentTypes>() {
		//		return std::make_tuple(get_entity_data<ParentTypes>(pid, pools)...);
		//	});

		//return parent_type{pid, tup_parent_ptrs};

	} else {
		// Standard: return the component from the pool
		return get_pool<T>(pools).find_component_data(entity);
	}
}

// Extracts a component argument from a tuple
template <typename Component, typename Tuple>
decltype(auto) extract_arg(Tuple& tuple, [[maybe_unused]] ptrdiff_t offset) {
	using T = std::remove_cvref_t<Component>;

	if constexpr (std::is_pointer_v<T>) {
		return nullptr;
	} else if constexpr (detail::unbound<T>) {
		T* ptr = std::get<T*>(tuple);
		return *ptr;
	} else if constexpr (detail::is_parent<T>::value) {
		return std::get<T>(tuple);
	} else {
		T* ptr = std::get<T*>(tuple);
		return *(ptr + offset);
	}
}

// Extracts a component argument from a pointer+offset
template <typename Component>
decltype(auto) extract_arg_lambda(auto& cmp, [[maybe_unused]] ptrdiff_t offset, [[maybe_unused]] auto pools = std::ptrdiff_t{0}) {
	using T = std::remove_cvref_t<Component>;

	if constexpr (std::is_pointer_v<T>) {
		return static_cast<T>(nullptr);
	} else if constexpr (detail::unbound<T>) {
		T* ptr = cmp;
		return *ptr;
	} else if constexpr (detail::is_parent<T>::value) {
		parent_id const pid = *(cmp + offset);

		// TODO store this in seperate container in system_hierarchy? might not be
		//      needed after O(1) pool lookup implementation
		using parent_type = std::remove_cvref_t<Component>;
		return apply_type<parent_type_list_t<parent_type>>([&]<typename... ParentTypes>() {
			return parent_type{pid, get_entity_data<ParentTypes>(pid, pools)...};
		});
	} else {
		T* ptr = cmp;
		return *(ptr + offset);
	}
}

// The type of a single component argument
template <typename Component>
using component_argument = std::conditional_t<is_parent<std::remove_cvref_t<Component>>::value,
											  std::remove_cvref_t<reduce_parent_t<Component>>*,	// parent components are stored as copies
											  std::remove_cvref_t<Component>*>; // rest are pointers
} // namespace ecs::detail

#endif // !ECS_SYSTEM_DEFS_H_

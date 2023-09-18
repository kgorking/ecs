#ifndef ECS_DETAIL_COMPONENT_POOLS_H
#define ECS_DETAIL_COMPONENT_POOLS_H

#include "system_defs.h"

namespace ecs::detail {

// Forward decls
class component_pool_base;
template <typename, typename> class component_pool;

// Holds a bunch of component pools that can be looked up based on a type.
// The types in the type_list must be naked, so no 'int&' or 'char*' allowed.
template <impl::TypeList ComponentsList>
	requires(std::is_same_v<ComponentsList, transform_type<ComponentsList, naked_component_t>>)
struct component_pools {
	explicit constexpr component_pools(auto... pools) noexcept : base_pools{pools...} {
		Pre((pools != nullptr) && ...);
	}

	// Get arguments corresponding component pool.
	// The type must be naked, ie. 'int' and not 'int&'.
	// This is to prevent the compiler from creating a bajillion
	// different instances that all do the same thing.
	template <typename Component>
		requires(std::is_same_v<Component, naked_component_t<Component>>)
	constexpr auto& get() const noexcept {
		constexpr int index = index_of<Component, ComponentsList>();
		return *static_cast<component_pool<Component>*>(base_pools[index]);
	}

	constexpr bool has_component_count_changed() const {
		return any_of_type<ComponentsList>([this]<typename T>() {
			return this->get<T>().has_component_count_changed();
		});
	}

private:
	component_pool_base* base_pools[type_list_size<ComponentsList>];
};




// Get an entities component from a component pool
template <typename Component, impl::TypeList PoolsList>
[[nodiscard]] auto get_component(entity_id const entity, component_pools<PoolsList> const& pools) {
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
		return &pools.template get<T>().get_shared_component();
	} else if constexpr (std::is_same_v<reduce_parent_t<T>, parent_id>) {
		return pools.template get<parent_id>().find_component_data(entity);
	} else {
		// Standard: return the component from the pool
		return pools.template get<T>().find_component_data(entity);
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
		return for_all_types<parent_type_list_t<T>>([&]<typename... ParentTypes>() {
			return T{pid, get_component<ParentTypes>(pid, pools)...};
		});
	} else {
		T* ptr = cmp;
		return *(ptr + offset);
	}
}

}

#endif //!ECS_DETAIL_COMPONENT_POOLS_H

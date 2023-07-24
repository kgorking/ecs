#ifndef ECS_DETAIL_COMPONENT_POOLS_H
#define ECS_DETAIL_COMPONENT_POOLS_H

#include "system_defs.h"

namespace ecs::detail {

// Forward decls
class component_pool_base;
template <typename, typename> class component_pool;

template<typename T>
using modstrip = std::remove_pointer_t<std::remove_cvref_t<reduce_parent_t<T>>>;

// 
template <typename ComponentsList>
struct component_pools : type_list_indices<transform_type<ComponentsList, modstrip>> {
	constexpr component_pools() noexcept = default;
	constexpr component_pools(component_pools const&) noexcept = default;
	constexpr component_pools(component_pools &&) noexcept = default;
	constexpr component_pools& operator=(component_pools const&) noexcept = default;
	constexpr component_pools& operator=(component_pools&&) noexcept = default;
	constexpr ~component_pools() noexcept = default;

	explicit constexpr component_pools(auto... pools) noexcept : base_pools{pools...} {
		Expects((pools != nullptr) && ...);
	}

	template <typename Component>
	constexpr auto& get() const noexcept {
		using T = std::remove_pointer_t<std::remove_cvref_t<reduce_parent_t<Component>>>;
		constexpr int index = component_pools::index_of(static_cast<impl::wrap_t<T>*>(nullptr));
		return *static_cast<component_pool<T>*>(base_pools[index]);
	}

	constexpr bool has_component_count_changed() const {
		return any_of_type<ComponentsList>([this]<typename T>() {
			return this->get<T>().has_component_count_changed();
		});
	}

private:
	component_pool_base* base_pools[type_list_size<ComponentsList>];
};

}

#endif //!ECS_DETAIL_COMPONENT_POOLS_H

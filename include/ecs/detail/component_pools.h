#ifndef ECS_DETAIL_COMPONENT_POOLS_H
#define ECS_DETAIL_COMPONENT_POOLS_H

namespace ecs::detail {

// Forward decls
class component_pool_base;
template <typename, typename> class component_pool;

// 
template <typename ComponentsList>
struct component_pools : type_list_indices<ComponentsList> {
	component_pool_base* base_pools[type_list_size<ComponentsList>];

	constexpr component_pools(auto... pools) noexcept : base_pools{pools...} {
		Expects((pools != nullptr) && ...);
	}

	template <typename Component>
	requires (!std::is_reference_v<Component> && !std::is_pointer_v<Component>)
	constexpr auto& get() const noexcept {
		constexpr int index = index_of<Component, type_list_indices<ComponentsList>>();
		return *static_cast<component_pool<Component>*>(base_pools[index]);
	}

	constexpr bool has_component_count_changed() const {
		return any_of_type<ComponentsList>([this]<typename T>() {
			return this->get<T>().has_component_count_changed();
		});
	}
};


}


#endif //!ECS_DETAIL_COMPONENT_POOLS_H

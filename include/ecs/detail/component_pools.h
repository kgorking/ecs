#ifndef ECS_DETAIL_COMPONENT_POOLS_H
#define ECS_DETAIL_COMPONENT_POOLS_H

namespace ecs::detail {

// Forward decls
class component_pool_base;
template <typename, typename> class component_pool;

// 
template <typename ComponentsList>
struct component_pools : type_list_indices<ComponentsList> {
	constexpr component_pools(auto... pools) noexcept : base_pools{pools...} {
		Expects((pools != nullptr) && ...);
	}

	template <typename Component>
	constexpr auto& get() const noexcept {
		constexpr int index = component_pools::index_of(static_cast<Component*>(nullptr));
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

}

#endif //!ECS_DETAIL_COMPONENT_POOLS_H

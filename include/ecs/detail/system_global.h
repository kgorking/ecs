#ifndef ECS_DETAIL_SYSTEM_GLOBAL_H
#define ECS_DETAIL_SYSTEM_GLOBAL_H

#include "system.h"

namespace ecs::detail {
// The implementation of a system specialized on its components
template <typename Options, typename UpdateFn, bool FirstIsEntity, typename ComponentsList, typename PoolsList>
class system_global final : public system<Options, UpdateFn, FirstIsEntity, ComponentsList, PoolsList> {
	using base = system<Options, UpdateFn, FirstIsEntity, ComponentsList, PoolsList>;

public:
	system_global(UpdateFn func, component_pools<PoolsList>&& in_pools)
		: base{func, std::forward<component_pools<PoolsList>>(in_pools)} {
		this->process_changes(true);
	  }

private:
	void do_run() override {
		for_all_types<PoolsList>([&]<typename... Types>() {
			this->update_func(this->pools.template get<Types>().get_shared_component()...);
		});
	}

	void do_build() override {
	}
};
} // namespace ecs::detail

#endif // !ECS_DETAIL_SYSTEM_GLOBAL_H

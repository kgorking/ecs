#ifndef ECS_SYSTEM_GLOBAL_H
#define ECS_SYSTEM_GLOBAL_H

#include "system.h"

namespace ecs::detail {
// The implementation of a system specialized on its components
template <typename Options, typename UpdateFn, typename TupPools, bool FirstIsEntity, typename ComponentsList>
class system_global final : public system<Options, UpdateFn, TupPools, FirstIsEntity, ComponentsList> {
public:
	system_global(UpdateFn func, TupPools in_pools)
		: system<Options, UpdateFn, TupPools, FirstIsEntity, ComponentsList>{func, in_pools} {
		this->process_changes(true);
	  }

private:
	void do_run() override {
		for_all_types<ComponentsList>([&]<typename... Types>(){
			this->update_func(get_pool<Types>(this->pools).get_shared_component()...);
		});
	}

	void do_build() override {
	}
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_GLOBAL_H

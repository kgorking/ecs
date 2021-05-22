#ifndef ECS_SYSTEM_GLOBAL_H
#define ECS_SYSTEM_GLOBAL_H

#include "system.h"

namespace ecs::detail {
// The implementation of a system specialized on its components
template <class Options, class UpdateFn, class TupPools, class FirstComponent, class... Components>
class system_global final : public system<Options, UpdateFn, TupPools, FirstComponent, Components...> {
public:
	system_global(UpdateFn func, TupPools in_pools)
		: system<Options, UpdateFn, TupPools, FirstComponent, Components...>{func, in_pools},
		  argument{&get_pool<FirstComponent>(in_pools).get_shared_component(), &get_pool<Components>(in_pools).get_shared_component()...} {}

private:
	void do_run() override {
		this->update_func(*std::get<std::remove_cvref_t<FirstComponent>*>(argument),
						  *std::get<std::remove_cvref_t<Components>*>(argument)...);
	}

	void do_build(entity_range_view) override {
		// Does nothing
	}

private:
	// The arguments for the system
	using global_argument = std::tuple<std::remove_cvref_t<FirstComponent>*, std::remove_cvref_t<Components>*...>;
	global_argument argument;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_GLOBAL_H

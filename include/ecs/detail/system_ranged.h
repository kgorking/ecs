#ifndef ECS_SYSTEM_RANGED_H
#define ECS_SYSTEM_RANGED_H

#include "pool_range_walker.h"
#include "system.h"

namespace ecs::detail {
// Manages arguments using ranges. Very fast linear traversal and minimal storage overhead.
template <class Options, class UpdateFn, class TupPools, class FirstComponent, class... Components>
class system_ranged final : public system<Options, UpdateFn, TupPools, FirstComponent, Components...> {
	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

public:
	system_ranged(UpdateFn func, TupPools in_pools)
		: system<Options, UpdateFn, TupPools, FirstComponent, Components...>{func, in_pools}, walker{in_pools} {}

private:
	void do_run() override {
		job();
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build(entity_range_view entities) override {
		// Reset the walker
		walker.reset(entities);

		std::vector<argument_type> arguments;
		while (!walker.done()) {
			if constexpr (is_entity<FirstComponent>) {
				arguments.emplace_back(walker.get_range(), walker.template get<Components>()...);
			} else {
				arguments.emplace_back(walker.get_range(), walker.template get<FirstComponent>(), walker.template get<Components>()...);
			}

			walker.next();
		}

		job = scheduler_job([arguments = std::move(arguments), func = this->update_func]() mutable {
			auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
			// Call the system for all the components that match the system signature
			for (auto const& argument : arguments) {
				entity_range const& range = std::get<entity_range>(argument);
				std::for_each(e_p, range.begin(), range.end(), [&, first_id = range.first()](entity_id ent) {
					auto const offset = ent - first_id;

					if constexpr (is_entity<FirstComponent>) {
						func(ent, extract_arg<Components>(argument, offset)...);
					} else {
						func(extract_arg<FirstComponent>(argument, offset), extract_arg<Components>(argument, offset)...);
					}
				});
			}
		});
	}

private:
	// Holds the arguments for a range of entities
	using argument_type = range_argument<FirstComponent, Components...>;
	scheduler_job job;

	pool_range_walker<TupPools> walker;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_RANGED_H

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
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build(entity_range_view entities) override {
		// Reset the walker
		walker.reset(entities);

		total_arguments = 0;

		while (!walker.done()) {
			total_arguments += walker.get_range().count();

			if constexpr (is_entity<FirstComponent>) {
				arguments.emplace_back(walker.get_range(), walker.template get<Components>()...);
			} else {
				arguments.emplace_back(walker.get_range(), walker.template get<FirstComponent>(), walker.template get<Components>()...);
			}

			walker.next();
		}

		set_jobs_done(false);
	}

	void do_job_generation(scheduler &s, jobs_layout &layout) override {
		if (arguments.size() == 1 && std::get<entity_range>(arguments[0]).count() < std::thread::hardware_concurrency()) {
			entity_range const& range = std::get<entity_range>(arguments[0]);
			auto first_id = range.first();

			for(entity_id ent : range) {
				auto const offset = ent - first_id;

				s.submit_job(layout, [ent, offset, func = this->update_func, arg = arguments[0]]() {
					if constexpr (is_entity<FirstComponent>) {
						func(ent, extract_arg<Components>(arg, offset)...);
					} else {
						func(extract_arg<FirstComponent>(arg, offset), extract_arg<Components>(arg, offset)...);
					}
				});
			}
		}
		else {
			s.submit_job(layout, [arguments = std::move(arguments), func = this->update_func]() mutable {
				// Call the system for all the components that match the system signature
				for (auto const& argument : arguments) {
					entity_range const& range = std::get<entity_range>(argument);
					std::for_each(range.begin(), range.end(), [&, first_id = range.first()](entity_id ent) {
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

		set_jobs_done(true);
	}

private:
	using system_base::set_jobs_done;
	using system_base::get_jobs_done;

	// Holds the arguments for a range of entities
	using argument_type = range_argument<FirstComponent, Components...>;
	std::vector<argument_type> arguments;
	size_t total_arguments{0};

	pool_range_walker<TupPools> walker;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_RANGED_H

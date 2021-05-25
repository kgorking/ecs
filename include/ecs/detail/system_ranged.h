#ifndef ECS_SYSTEM_RANGED_H
#define ECS_SYSTEM_RANGED_H

#include "pool_range_walker.h"
#include "system.h"

namespace ecs::detail {
// Manages arguments using ranges. Very fast linear traversal and minimal storage overhead.
template <class Options, class UpdateFn, class TupPools, class FirstComponent, class... Components>
class system_ranged final : public system<Options, UpdateFn, TupPools, FirstComponent, Components...> {
	using super = system<Options, UpdateFn, TupPools, FirstComponent, Components...>;

	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

public:
	system_ranged(UpdateFn func, TupPools in_pools)
		: system<Options, UpdateFn, TupPools, FirstComponent, Components...>{func, in_pools}, walker{in_pools} {}

private:
	void do_run() override {}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build(entity_range_view) override {}

	auto get_argument_from_walker() const {
		if constexpr (is_entity<FirstComponent>) {
			return argument_type{walker.get_range(), walker.template get<Components>()...};
		} else {
			return argument_type(walker.get_range(), walker.template get<FirstComponent>(), walker.template get<Components>()...);
		}
	}

	void do_job_generation(scheduler& scheduler) override {
		set_jobs_done(false);
		clear_job_details();

		// Find the entities this system spans
		std::vector<entity_range> entities = super::find_entities();

		if (has_predecessors()) {
			for (system_base* const pre : get_predecessors()) {
				// get all ranges
				std::vector<entity_range> predecessor_ranges;
				for (auto const detail : pre->get_job_details()) {
					predecessor_ranges.push_back(detail.entities);
				}
				Expects(predecessor_ranges.size() > 0);

				// Find the overlapping ranges
				predecessor_ranges = intersect_ranges(predecessor_ranges, entities);

				// build args + job_location
				if (predecessor_ranges.size()) {
					walker.reset(predecessor_ranges);
					for (auto const detail : pre->get_job_details()) {
						// skip ahead to predecessor ranges if needed
						if (detail.entities < walker.get_range())
							continue;

						// move predecessor to this range
						while (!walker.done() && walker.get_range() < detail.entities)
							walker.next();

						if (walker.done())
							break;

						auto const argument = get_argument_from_walker();

						job_location const loc = scheduler.submit_job(
							[argument, func = this->update_func]() {
								entity_range const range = std::get<entity_range>(argument);
								for (entity_id ent : range) {
									auto const offset = ent - range.first();

									if constexpr (is_entity<FirstComponent>) {
										func(ent, extract_arg<Components>(argument, offset)...);
									} else {
										func(extract_arg<FirstComponent>(argument, offset), extract_arg<Components>(argument, offset)...);
									}
								}
							},
							detail.loc.thread_index);

						add_job_detail(walker.get_range(), loc);
					}
				}

				// Removed the processed ranges
				entities = difference_ranges(entities, predecessor_ranges);
			}
		}
		// Reset the walker and build remaining args
		walker.reset(entities);

		if (entities.size() == 1 && entities.front().count() < std::thread::hardware_concurrency()) {
			entity_range const& range = entities.front();
			auto first_id = range.first();

			for (entity_id ent : range) {
				auto const offset = ent - first_id;

				job_location const loc = scheduler.submit_job([ent, offset, func = this->update_func, arg = get_argument_from_walker()]() {
					if constexpr (is_entity<FirstComponent>) {
						func(ent, extract_arg<Components>(arg, offset)...);
					} else {
						func(extract_arg<FirstComponent>(arg, offset), extract_arg<Components>(arg, offset)...);
					}
				});

				add_job_detail({ent, ent}, loc);
			}
		} else {
			while (!walker.done()) {
				auto argument = get_argument_from_walker();

				entity_range const range = std::get<entity_range>(argument);
				job_location const loc = scheduler.submit_job([argument = std::move(argument), func = this->update_func]() mutable {
					entity_range const range = std::get<entity_range>(argument);
					for (entity_id ent : range) {
						auto const offset = ent - range.first();

						if constexpr (is_entity<FirstComponent>) {
							func(ent, extract_arg<Components>(argument, offset)...);
						} else {
							func(extract_arg<FirstComponent>(argument, offset), extract_arg<Components>(argument, offset)...);
						}
					}
				});
				add_job_detail(range, loc);

				walker.next();
			}
		}

		set_jobs_done(true);
	}

private:
	using system_base::add_job_detail;
	using system_base::clear_job_details;
	using system_base::get_job_details;
	using system_base::get_jobs_done;
	using system_base::get_predecessors;
	using system_base::has_predecessors;
	using system_base::set_jobs_done;

	// Holds the arguments for a range of entities
	using argument_type = range_argument<FirstComponent, Components...>;

	pool_range_walker<TupPools> walker;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_RANGED_H

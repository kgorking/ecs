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

	// Holds the arguments for a range of entities
	using argument_type = range_argument<FirstComponent, Components...>;

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

	void submit_range(scheduler& scheduler, argument_type argument, int thread_index = -1) {
		entity_range const range = std::get<entity_range>(argument);
		job_location const loc = scheduler.submit_job(
			[argument, range, func = this->update_func]() mutable {
				for (entity_id ent : range) {
					auto const offset = ent - range.first();

					if constexpr (is_entity<FirstComponent>) {
						func(ent, extract_arg<Components>(argument, offset)...);
					} else {
						func(extract_arg<FirstComponent>(argument, offset), extract_arg<Components>(argument, offset)...);
					}
				}
			},
			thread_index);

		system_base::add_job_detail(range, loc);
	}

	void submit_individual(scheduler& scheduler, argument_type argument, int thread_index = -1) {
		entity_range const range = std::get<entity_range>(argument);
		for (entity_id ent : range) {
			job_location const loc = scheduler.submit_job(
				[ent, argument, range, func = this->update_func]() mutable {
					auto const offset = ent - range.first();

					if constexpr (is_entity<FirstComponent>) {
						func(ent, extract_arg<Components>(argument, offset)...);
					} else {
						func(extract_arg<FirstComponent>(argument, offset), extract_arg<Components>(argument, offset)...);
					}
				},
				thread_index);
			system_base::add_job_detail(entity_range{ent,ent}, loc);
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
				for (auto const job_d : pre->get_job_details()) {
					predecessor_ranges.push_back(job_d.entities);
				}
				
				if (predecessor_ranges.empty())
					break;

				// Find the overlapping ranges
				predecessor_ranges = intersect_ranges(predecessor_ranges, entities);

				// build args + job_location
				if (predecessor_ranges.size()) {
					walker.reset(predecessor_ranges);
					for (auto const job_d : pre->get_job_details()) {
						// skip this range ahead to predecessor ranges if needed
						if (job_d.entities < walker.get_range())
							continue;

						// skip predecessor range to this range
						while (!walker.done() && walker.get_range() < job_d.entities)
							walker.next();

						if (walker.done())
							break;

						submit_range(scheduler, get_argument_from_walker(), job_d.loc.thread_index);
					}
				}

				// Removed the processed ranges
				entities = difference_ranges(entities, predecessor_ranges);
			}
		}

		// Reset the walker and build remaining args
		walker.reset(entities);

		// Count the entities
		size_t num_entities = 0;
		for (auto const& range : entities) {
			num_entities += range.count();
		}

		if (num_entities <= MAX_THREADS) {
			while (!walker.done()) {
				argument_type const argument = get_argument_from_walker();
				submit_individual(scheduler, argument);
				walker.next();
			}
		} else {
			// Batch sizes?
			size_t const batch_size = num_entities / MAX_THREADS;

			while (!walker.done()) {
				argument_type argument = get_argument_from_walker();
				entity_range &range = std::get<entity_range>(argument); // REF

				if (range.count() <= batch_size) {
					submit_range(scheduler, argument);
				} else {
					// large batch, so split it up
					while (range.count() > batch_size+1) {
						// Split the range.
						auto const next_range = range.split(static_cast<int>(batch_size));

						// Submit the work
						submit_range(scheduler, argument);

						// Advance the pointers in the arguments
						std::apply(
							[batch_size](auto &... args) {
								auto const advancer = [&](auto &arg) { 
									if constexpr (std::is_pointer_v<decltype(arg)>) {
										arg += batch_size;
									};
								};

								(advancer(args), ...);
							}, argument);

						// Update the range
						range = next_range;
					}

					// Submit the remaining work
					submit_range(scheduler, argument);
				}

				walker.next();
			}
		}

		set_jobs_done(true);
	}

private:
	using system_base::clear_job_details;
	using system_base::get_job_details;
	using system_base::get_jobs_done;
	using system_base::get_predecessors;
	using system_base::has_predecessors;
	using system_base::set_jobs_done;

	pool_range_walker<TupPools> walker;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_RANGED_H

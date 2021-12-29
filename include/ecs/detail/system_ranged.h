#ifndef ECS_SYSTEM_RANGED_H
#define ECS_SYSTEM_RANGED_H

#ifdef ECS_SCHEDULER_LAYOUT_DEMO
//#include <format>
#include <iostream>
#endif
#include <ranges>
#include <unordered_set>

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

	job_location submit_range(scheduler& scheduler, argument_type argument, job_location const* from = nullptr) {
		entity_range const range = std::get<entity_range>(argument);
		job_location const loc = scheduler.submit_job_ranged(range, argument,
			[first = range.first(), this](entity_id ent, auto& argument) mutable {
				auto const offset = ent - first;

				if constexpr (is_entity<FirstComponent>) {
					super::update_func(ent, extract_arg<Components>(argument, offset)...);
				} else {
					super::update_func(extract_arg<FirstComponent>(argument, offset), extract_arg<Components>(argument, offset)...);
				}
			}, from);
		system_base::add_job_detail(range, loc);

		// Link jobs
		if (from != nullptr) {
			link_jobs(scheduler, *from, loc);
		}

		return loc;
	}

	void submit_individual(scheduler& scheduler, argument_type argument) {
		entity_range const range = std::get<entity_range>(argument);
		for (entity_id ent : range) {
			job_location const loc = scheduler.submit_job_ranged(
				{ent, ent}, argument, [first = range.first(), this](entity_id const ent, auto& argument) mutable {
					auto const offset = ent - first;

					if constexpr (is_entity<FirstComponent>) {
						super::update_func(ent, extract_arg<Components>(argument, offset)...);
					} else {
						super::update_func(extract_arg<FirstComponent>(argument, offset), extract_arg<Components>(argument, offset)...);
					}
				});
			system_base::add_job_detail(entity_range{ent, ent}, loc);
		}
	}

	void link_jobs(scheduler& scheduler, job_location const& from, job_location const& to) {
#ifdef ECS_SCHEDULER_LAYOUT_DEMO
		//std::cout << '(' << from.thread_index << "," << from.job_position << ") -> (" << to.thread_index << "," << to.job_position << "): ";
		std::cout << std::format("({},{}) -> ({},{}): ", from.thread_index,
								 from.job_position, to.thread_index, to.job_position);
#endif
		// Dont synchronize on same thread
		if (from.thread_index == to.thread_index) {
#ifdef ECS_SCHEDULER_LAYOUT_DEMO
			std::cout << "on same thread; do nothing\n";
#endif
			return;
		}

		auto& from_job = scheduler.get_job(from);
		auto& to_job = scheduler.get_job(to);

		to_job.increase_incoming_job_count();
		from_job.add_outgoing_barrier(to_job.get_barrier());
#ifdef ECS_SCHEDULER_LAYOUT_DEMO
		std::cout << "\n";
#endif
	}

	// Generate jobs for entities that overlap predecessors
	void predecessor_job_generation(scheduler& scheduler, std::vector<entity_range>& entities) {
		// Remember what thread each range is scheduled on
		// TODO stick in scheduler_context
		std::map<entity_range, job_location> range_thread_map;

		// Get any work from the predecessor that overlaps the
		// entity range of this system
		for (system_base* const pre : get_predecessors()) {
			// get all entity ranges
			std::vector<entity_range> predecessor_ranges;
			for (auto const job_d : pre->get_job_details()) {
				predecessor_ranges.push_back(job_d.entities);
			}

			// Find the overlapping ranges
			predecessor_ranges = intersect_ranges(predecessor_ranges, entities);

			// If there are no overlaps between the predecessors
			// entity ranges and this systems ranges, do nothing.
			if (predecessor_ranges.empty()) {
				continue;
			}

			// Build args
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

				auto const map = range_thread_map.find(walker.get_range());
				if (map == range_thread_map.end()) {
					job_location const loc = submit_range(scheduler, get_argument_from_walker());
					range_thread_map[walker.get_range()] = loc;

					// 
					link_jobs(scheduler, job_d.loc, loc);
				} else {
					link_jobs(scheduler, job_d.loc, map->second);
				}
			}
		}

		// Remove the processed ranges
		for (auto const [range, _] : range_thread_map) {
			entities = difference_ranges(entities, {&range, 1});
		}
	}

	void job_generation(scheduler& scheduler, std::vector<entity_range>& entities) {
		// Reset the walker and build remaining non-dependant args
		walker.reset(entities);

		// Count the entities
		size_t num_entities = 0;
		for (auto const& range : entities) {
			num_entities += range.count();
		}

		if (num_entities <= MAX_THREADS) {
			while (!walker.done()) {
				num_entities -= walker.get_range().count();
				argument_type const argument = get_argument_from_walker();
				submit_individual(scheduler, argument);
				walker.next();
			}
		} else {
			// Batch sizes?
			size_t const batch_size = num_entities / MAX_THREADS;

			while (!walker.done()) {
				argument_type argument = get_argument_from_walker();
				entity_range& range = std::get<entity_range>(argument); // REF

				if (true/*range.count() <= batch_size*/) {
					num_entities -= range.count();
					submit_range(scheduler, argument);
				} else {
					// large batch, so split it up
					while (range.ucount() > batch_size) {
						// Split the range.
						auto const next_range = range.split(static_cast<int>(batch_size));

						// Submit the work
						num_entities -= range.count();
						submit_range(scheduler, argument);

						// Advance the pointers in the arguments
						std::apply(
							[batch_size](auto&... args) {
								auto const advancer = [&](auto& arg) {
									if constexpr (std::is_pointer_v<std::remove_reference_t<decltype(arg)>>) {
										arg += batch_size;
									};
								};

								(advancer(args), ...);
							},
							argument);

						// Update the range
						range = next_range;
					}

					// Submit the remaining work
					submit_range(scheduler, argument);
					num_entities -= range.count();
				}

				walker.next();
			}

			Expects(num_entities == 0);
		}
	}

	void do_job_generation(scheduler& scheduler) override {
		// Find the entities this system spans
		std::vector<entity_range> entities = super::find_entities();
		if (entities.size() == 0) {
			// No entities, so bail
			clear_job_details();
			set_jobs_done(true);
			return;
		}

		set_jobs_done(false);
		clear_job_details();

		if (has_predecessors()) {
			predecessor_job_generation(scheduler, entities);
		}

		job_generation(scheduler, entities);

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

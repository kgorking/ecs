#ifndef ECS_DETAIL_STATIC_SCHEDULER_H
#define ECS_DETAIL_STATIC_SCHEDULER_H

#include <set>
#include <thread>
#include <vector>
#include "entity_range.h"
#include "operation.h"
#include "system_base.h"

namespace ecs::detail {
	inline auto fuse_ops(operation a, operation b) {
		return [=](entity_id id, entity_offset offset) {
			a.run(id, offset);
			b.run(id, offset);
		};
	}

	inline auto fuse_ops(operation a, auto arg, auto func) {
		return [=](entity_id id, entity_offset offset) {
			a.run(id, offset);
			(*arg)(*func, id, offset);
		};
	}

	class job final {
		//entity_range range;
		operation op;
	};

	struct pipeline final {
		// The vector of jobs to run on this thread
		std::vector<job> jobs;

		// The thread to execute the pipeline on
		std::thread thread;

		// Time it took to run all jobs
		double time = 0.0;
	};

	struct static_scheduler final {
		void insert(system_base* sys) {
			systems.push_back(sys);
		}

		void build() {
			for (system_base* sys : systems)
				sys->clear_dependencies();

			// Find all system dependencies
			for (auto it = systems.rbegin(); it != systems.rend(); ++it) {
				system_base* sys = *it;

				for (auto prev_it = std::next(it); prev_it != systems.rend(); ++prev_it) {
					system_base* prev_sys = *prev_it;

					for (auto const hash : sys->get_type_hashes()) {
						if (prev_sys->has_component(hash)) {
							sys->add_dependency(prev_sys);
							break;
						}
					}
				}
			}

			// Get the systems needed
			for (system_base* sys : systems) {
				std::vector<system_base*> v;
				std::unordered_set<system_base*> visited;
				sys->get_flattened_dependencies(v, visited);
			}
		}

	private:
		std::vector<system_base*> systems;
	};

} // namespace ecs::detail

#endif // !ECS_DETAIL_STATIC_SCHEDULER_H

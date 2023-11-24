#ifndef ECS_DETAIL_STATIC_SCHEDULER_H
#define ECS_DETAIL_STATIC_SCHEDULER_H

#include "entity_range.h"
#include <thread>
#include <vector>

namespace ecs::detail {
	struct operation final {
		template <typename Arguments, typename Fn>
		explicit operation(Arguments& args, Fn& fn)
			: arguments{&args}, function(&fn), op{[](entity_id id, entity_offset offset, void* p1, void* p2) {
				  auto* arg = static_cast<Arguments*>(p1);
				  auto* func = static_cast<Fn*>(p2);
				  (*arg)(*func, id, offset);
			  }} {}

		void run(entity_id id, entity_offset offset) const {
			op(id, offset, arguments, function);
		}

	private:
		void* arguments;
		void* function;
		void (*op)(entity_id id, entity_offset offset, void*, void*);
	};

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
		entity_range range;
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

			// Find a dependant system for each component
			for (auto it = systems.rbegin(); it != systems.rend(); ++it) {
				system_base* sys = *it;

				for (auto dep_it = std::next(it); dep_it != systems.rend(); ++dep_it) {
					system_base* dep_sys = *dep_it;

					for (auto const hash : sys->get_type_hashes()) {
						if (dep_sys->writes_to_component(hash) || sys->writes_to_component(hash)) {
							// The system writes to the component,
							// so there is a strong dependency here.
							sys->add_dependency(dep_sys);
							break;
						} else { // 'other' reads component
									// These systems have a weak read/read dependency
									// and can be scheduled concurrently
						}
					}
				}
			}

			// TODO
		}

	private:
		std::vector<system_base*> systems;
	};

} // namespace ecs::detail

#endif // !ECS_DETAIL_STATIC_SCHEDULER_H

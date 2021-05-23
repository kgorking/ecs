#ifndef ECS_SYSTEM_SCHEDULER
#define ECS_SYSTEM_SCHEDULER

#include <algorithm>
#include <semaphore>
#include <barrier>
#include <thread>
#include <vector>

#include "contract.h"
#include "scheduler_job.h"
#include "system_base.h"

namespace ecs::detail {

// Schedules systems for concurrent execution based on their components.
class scheduler final {
	// A group of systems with the same group id
	struct systems_group final {
		int id;
		std::vector<system_base*> all_nodes;
		std::vector<system_base*> entry_nodes{};

		// Runs the entry nodes in parallel
		void run() {}
	};

	std::vector<systems_group> groups;

	std::vector<std::jthread> threads;
	std::vector<std::vector<scheduler_job>> jobs;

	// Semaphore used to tell threads to start their work
	std::counting_semaphore<> smph_start_signal{0};

	// Barrier used to tell the main thread when the workers are done
	std::barrier<> smph_workers_finished{1 + std::thread::hardware_concurrency()};

protected:
	void worker_thread() {
		// Wait for a signal from the main thread
		smph_start_signal.acquire();

		// Signal the main thread back
		smph_workers_finished.arrive_and_wait();
	}

	systems_group& find_group(int id) {
		// Look for an existing group
		if (!groups.empty()) {
			for (auto& g : groups) {
				if (g.id == id) {
					return g;
				}
			}
		}

		// No group found, so find an insertion point
		auto const insert_point =
			std::upper_bound(groups.begin(), groups.end(), id, [](int group_id, systems_group const& sg) { return group_id < sg.id; });

		// Insert the group and return it
		return *groups.insert(insert_point, systems_group{id, {}, {}});
	}

public:
	scheduler() {
		for(unsigned i=0; i<std::thread::hardware_concurrency(); i++) {
			threads.emplace_back(&scheduler::worker_thread, this);
		}
	}

	void insert(system_base* sys) {
		// Find the group
		auto& group = find_group(sys->get_group());

		// Create a new node with the system
		group.all_nodes.push_back(sys);

		// Find a dependant system for each component
		bool inserted = false;
		auto const end = group.all_nodes.rend();
		for (auto const hash : sys->get_type_hashes()) {
			auto it = std::next(group.all_nodes.rbegin()); // 'next' to skip the newly added system
			while (it != end) {
				system_base* dep_sys = *it;
				// If the other system doesn't touch the same component,
				// then there can be no dependecy
				if (dep_sys->has_component(hash)) {
					if (dep_sys->writes_to_component(hash) || sys->writes_to_component(hash)) {
						// The system writes to the component,
						// so there is a strong dependency here.
						inserted = true;
						dep_sys->add_predecessor(sys);
						sys->add_sucessor(dep_sys);
						break;
					} else { // 'other' reads component
							 // These systems have a weak read/read dependency
							 // and can be scheduled concurrently
					}
				}

				++it;
			}
		}

		// The system has no dependencies, so make it an entry node
		if (!inserted) {
			group.entry_nodes.push_back(sys);
		}
	}

	// Clears all the schedulers data
	void clear() {
		groups.clear();
		jobs.clear();
	}

	// Build the schedule
	void make() {
		for (systems_group& group : groups) {
		}
	}

	void run() {
		// Tell threads to start working
		smph_start_signal.release(std::thread::hardware_concurrency());

		// Wait for all threads to finish
		smph_workers_finished.arrive_and_wait();
	}
};

} // namespace ecs::detail

#endif // !ECS_SYSTEM_SCHEDULER

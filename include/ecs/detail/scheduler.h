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
	};

	struct jobs_layout {
		int start = 0;
		int count = 0;
	};

public:
	scheduler() : jobs(std::thread::hardware_concurrency()) {
		// Create threads
		for(unsigned i=0; i<std::thread::hardware_concurrency(); i++) {
			threads.emplace_back(&scheduler::worker_thread, this, i);
		}
	}

	~scheduler() {
		done = true;
		start_threads();
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
						dep_sys->add_sucessor(sys);
						sys->add_predecessor(dep_sys);
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
	void clear() noexcept {
		groups.clear();
		jobs.clear();
	}

	// Clears the schedulers jobs
	void clear_jobs() noexcept {
		for (auto &vec : jobs)
			vec.clear();
	}

	template<class F>
	void submit_job(F&& f) {
		jobs[current_job_index].emplace_back(std::forward<F>(f));

		current_job_index = (1 + current_job_index) % std::thread::hardware_concurrency();
		layout.count += 1;
	}

	// Build the schedule
	void make() {
		clear_jobs();
		layout.start = 0;
		for (systems_group const& group : groups) {
			for (system_base *sys : group.entry_nodes) {
				layout.count = 0;
				generate_sys_jobs(sys);
			}
		}
	}

	void run() {
		start_threads();
		std::this_thread::yield();
		wait_for_threads();
	}

protected:
	// Tell threads to start working
	void start_threads() {
		smph_start_signal.release(std::thread::hardware_concurrency());
	}

	// Wait for all threads to finish
	void wait_for_threads() {
		smph_workers_finished.arrive_and_wait();
	}

	void worker_thread(unsigned index) {
		while (true) {
			// Wait for a signal from the main thread
			smph_start_signal.acquire();

			// Bust out early if requested
			if (done) break;

			// Run the jobs
			for (auto &job : jobs[index]) {
				job();
			}

			// Signal the main thread back
			smph_workers_finished.arrive_and_wait();
		}
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

	void generate_sys_jobs(system_base *sys) {
		sys->do_job_generation(*this);

		for (system_base* sucessor : sys->get_sucessors()) {
			// Start from the index as out predecessor
			current_job_index = layout.start;

			// Recursively generate jobs
			generate_sys_jobs(sucessor);
		}
	}

private:
	std::vector<systems_group> groups;

	std::vector<std::jthread> threads;
	std::vector<std::vector<scheduler_job>> jobs;
	int current_job_index = 0;
	jobs_layout layout;

	// Semaphore used to tell threads to start their work
	std::counting_semaphore<> smph_start_signal{0};

	// Barrier used to tell the main thread when the workers are done
	std::barrier<> smph_workers_finished{1 + std::thread::hardware_concurrency()};

	bool done = false;
};

} // namespace ecs::detail

#endif // !ECS_SYSTEM_SCHEDULER

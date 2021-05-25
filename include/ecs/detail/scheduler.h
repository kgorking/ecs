#ifndef ECS_SYSTEM_SCHEDULER
#define ECS_SYSTEM_SCHEDULER

#include <algorithm>
#include <semaphore>
#include <barrier>
#include <thread>
#include <vector>

#include "contract.h"
#include "system_base.h"
#include "scheduler_job.h"
#include "job_detail.h"

namespace ecs::detail {

#define MAX_THREADS (std::thread::hardware_concurrency())

// Schedules systems for concurrent execution based on their components.
class scheduler final {
	// A group of systems with the same group id
	struct systems_group final {
		int id;
		std::vector<system_base*> all_nodes;
		std::vector<system_base*> entry_nodes{};
	};

public:
	scheduler() : jobs(MAX_THREADS) {
		// Create threads
		for (unsigned i = 0; i < MAX_THREADS; i++) {
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
	[[nodiscard]] job_location submit_job(F&& f, int thread_index = -1) {
		Expects(thread_index == -1 || thread_index < static_cast<int>(MAX_THREADS));

		job_location loc{0, 0};
		if (thread_index != -1) {
			loc.thread_index = thread_index;
			loc.job_position = static_cast<int>(jobs[thread_index].size());

			jobs[thread_index].emplace_back(std::forward<F>(f));
		} else {
			// Place the job the smallest vector
			auto const it = std::ranges::min_element(jobs, [](auto const& vl, auto const& vr) { return vl.size() < vr.size(); });
			auto const index = std::distance(jobs.begin(), it);

			loc.thread_index = static_cast<int>(index);
			loc.job_position = static_cast<int>(jobs[index].size());

			jobs[index].emplace_back(std::forward<F>(f));
		}
		return loc;
	}

	// Build the schedule
	void process_changes() {
		clear_jobs();
		for (systems_group const& group : groups) {
			for (system_base *sys : group.all_nodes) {
				// skip systems that are already done
				if (sys->get_jobs_done())
					continue;

				//auto const it = std::ranges::min_element(jobs, [](auto const& vl, auto const& vr) { return vl.size() < vr.size(); });
				generate_sys_jobs(sys);
			}
		}
	}

	void run() {
#ifdef ECS_SCHEDULER_LAYOUT_DEMO
		int index = 0;
		while (true) {
			size_t jobs_empty = 0;

			for (size_t thread_index = 0; thread_index < MAX_THREADS; thread_index++) {
				auto& job_vec = jobs.at(thread_index);
				if (index < job_vec.size()) {
					job_vec.at(index)();
				} else {
					jobs_empty += 1;
					putchar('-');
					putchar(' ');
				}
			}

			putchar('\n');
			index++;

			if (jobs_empty == MAX_THREADS)
				break;
		}
#else
		start_threads();
		std::this_thread::yield();
		wait_for_threads();
#endif
	}

protected:
	// Tell threads to start working
	void start_threads() noexcept {
		smph_start_signal.release(MAX_THREADS);
	}

	// Wait for all threads to finish
	void wait_for_threads() noexcept {
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
		// Leave if this system is already processed
		if (sys->get_jobs_done()) {
			return;
		} else {
			sys->set_jobs_done(true);
		}

		// Generate predecessors
		for (system_base* predecessor : sys->get_predecessors()) {
			generate_sys_jobs(predecessor);
		}

		// Generate the jobs, which will be filled in
		// according to the layout
		sys->do_job_generation(*this);

		// Generate successors
		for (system_base* sucessor : sys->get_sucessors()) {
			generate_sys_jobs(sucessor);
		}
	}

private:
	std::vector<systems_group> groups;

	std::vector<std::jthread> threads;
	std::vector<std::vector<scheduler_job>> jobs;

	// Semaphore used to tell threads to start their work
	std::counting_semaphore<> smph_start_signal{0};

	// Barrier used to tell the main thread when the workers are done
	std::barrier<> smph_workers_finished{1 + MAX_THREADS};

	bool done = false;
};

} // namespace ecs::detail

#endif // !ECS_SYSTEM_SCHEDULER

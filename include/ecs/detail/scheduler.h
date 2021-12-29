#ifndef ECS_SYSTEM_SCHEDULER
#define ECS_SYSTEM_SCHEDULER

#include <iostream>
#include <barrier>
#include <algorithm>
#include <thread>
#include <vector>
#include <unordered_map>

#include "contract.h"
#include "job_detail.h"
#include "scheduler_job.h"
#include "system_base.h"

// TODO /ORDER https://docs.microsoft.com/en-us/cpp/build/reference/order-put-functions-in-order?view=msvc-160
// TODO 11.2.1 Effective CPU Utilization

namespace ecs::detail {

#define MAX_THREADS static_cast<size_t>(std::thread::hardware_concurrency())

// Schedules systems for concurrent execution based on their components.
class scheduler final {
	// A group of systems with the same group id
	struct systems_group final {
		int id;
		std::vector<system_base*> all_nodes;
		std::vector<system_base*> entry_nodes{};
	};

public:
	scheduler() : jobs(MAX_THREADS), start_barrier(1 + MAX_THREADS), stop_barrier(1 + MAX_THREADS) {
		// Create threads and atomic flags
		threads.reserve(MAX_THREADS);
		for (size_t i = 0; i < MAX_THREADS; i++) {
			threads.emplace_back(&scheduler::worker_thread, this, i);
		}
	}

	~scheduler() {
		clear_jobs();
		start_threads(true);
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
						// break;
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
		type_thread_map.clear();
		done = false;
		clear_jobs();
	}

	// Clears the schedulers jobs
	void clear_jobs() noexcept {
		for (auto& vec : jobs)
			vec.clear();
	}

	template <class F>
	[[nodiscard]] job_location submit_job(F&& f) {
		job_location loc{0, 0};

		// Place the job the smallest vector
		auto const it = std::ranges::min_element(jobs, [](auto const& vl, auto const& vr) {
			return vl.size() < vr.size();
		});
		auto const thread_index = std::distance(jobs.begin(), it);

		loc.thread_index = static_cast<int>(thread_index);
		loc.job_position = static_cast<int>(jobs[thread_index].size());

		jobs[thread_index].emplace_back(std::forward<F>(f));
		return loc;
	}

	template <class F, class Args>
	[[nodiscard]] job_location submit_job_ranged(entity_range range, Args args, F&& f, job_location const* from = nullptr) {
		int thread_index = -1;
		if (from == nullptr) {
			thread_index = find_thread_index(range, args);

			// Place the job the smallest vector
			if (thread_index == -1) {
				auto const it = std::ranges::min_element(jobs, [](auto const& vl, auto const& vr) {
					return vl.size() < vr.size();
				});
				thread_index = static_cast<int>(std::distance(jobs.begin(), it));
			}
		} else {
			// Schedule the job on the same thread as another job
			thread_index = from->thread_index;
		}

		insert_type_thread_index(range, args, thread_index);

		job_location loc{0, 0};
		loc.thread_index = static_cast<int>(thread_index);
		loc.job_position = static_cast<int>(jobs[thread_index].size());

		jobs[thread_index].emplace_back(range, args, std::forward<F>(f));

		return loc;
	}

	// Get a job
	scheduler_job& get_job(job_location loc) {
		Expects(loc.thread_index < jobs.size());
		Expects(loc.job_position < jobs[loc.thread_index].size());

		return jobs[loc.thread_index][loc.job_position];
	}

	// Build the schedule
	void process_changes() {
		clear_jobs();
		for (systems_group const& group : groups) {
			for (system_base* sys : group.all_nodes) {
				// skip systems that are already done
				if (sys->get_jobs_done())
					continue;

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
#endif
	}

protected:
	// Tell threads to start working
	void start_threads(bool is_done = false) {
		// Notify all threads to begin
		start_barrier.arrive_and_wait();

		if (is_done)
			done = true;

		// Wait for all threads to finish
		stop_barrier.arrive_and_wait();
	}

	// The worker run on each thread
	void worker_thread(size_t thread_index) {
		while (!done) {
			// Wait for a signal from the main thread
			start_barrier.arrive_and_wait();

			// Run the jobs
			for (auto& job : jobs[thread_index]) {
				job();
			}

			// Signal the main thread back
			stop_barrier.arrive_and_wait();
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
		auto const insert_point = std::upper_bound(groups.begin(), groups.end(), id, [](int group_id, systems_group const& sg) {
			return group_id < sg.id;
		});

		// Insert the group and return it
		return *groups.insert(insert_point, systems_group{id, {}, {}});
	}

	void generate_sys_jobs(system_base* sys) {
		// Leave if this system is already processed
		if (sys->get_jobs_done()) {
			return;
		} else {
			sys->set_jobs_done(true);
		}

		// Generate the jobs, which will be filled in
		// according to the layout
		sys->do_job_generation(*this);
	}

	// Find a possible thread index for a type and range
	// template<class Args>
	int find_thread_index(entity_range range, auto args) {
		if (type_thread_map.size() == 0)
			return -1;

		auto const find_type_thread_index = [&](auto single_arg) {
			using arg_type = decltype(single_arg);

			if constexpr (std::is_pointer_v<arg_type>) {
				constexpr type_hash hash = get_type_hash<arg_type>();
				auto const it = type_thread_map.find(hash);
				if (it != type_thread_map.end()) {
					for (auto const& pair : it->second) {
						if (pair.first.overlaps(range))
							return pair.second;
					}
				}
			}

			return -1;
		};

		return std::apply(
			[&](auto... split_args) {
				int const indices[] = {find_type_thread_index(split_args)...};
				for (int const index : indices) {
					if (index != -1)
						return index;
				}

				return -1;
			},
			args);
	}

	// template<class Args>
	void insert_type_thread_index(entity_range range, auto args, int thread_index) {
		auto const insert_type_thread_index = [&](auto single_arg) {
			using arg_type = decltype(single_arg);

			if constexpr (std::is_pointer_v<arg_type>) {
				constexpr type_hash hash = get_type_hash<arg_type>();
				auto it = type_thread_map.find(hash);
				if (it != type_thread_map.end()) {
					for (auto const& pair : it->second) {
						if (pair.first.overlaps(range)) {
							// Type is already mapped, so do nothing
							return;
						}
					}
				} else {
					// Map new type to thread
					auto const result = type_thread_map.emplace(hash, std::vector<std::pair<entity_range, int>>{});
					Expects(result.second);
					it = result.first;
				}

				it->second.emplace_back(range, thread_index);
			}
		};

		std::apply(
			[&](auto... split_args) {
				(insert_type_thread_index(split_args), ...);
			},
			args);
	}

private:
	std::vector<systems_group> groups;

	std::vector<std::jthread> threads;
	std::vector<std::vector<scheduler_job>> jobs; // TODO pmr

	std::barrier<> start_barrier;
	std::barrier<> stop_barrier;

	// Cache of which threads has accessed which types and their ranges
	std::unordered_map<type_hash, std::vector<std::pair<entity_range, int>>> type_thread_map;

	bool done = false;
};

} // namespace ecs::detail

#endif // !ECS_SYSTEM_SCHEDULER

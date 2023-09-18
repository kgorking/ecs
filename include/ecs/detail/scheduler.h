#ifndef ECS_SYSTEM_SCHEDULER
#define ECS_SYSTEM_SCHEDULER

#include <algorithm>
#include <atomic>
#include <execution>
#include <vector>

#include "contract.h"
#include "system_base.h"

namespace ecs::detail {

// Describes a node in the scheduler execution graph
struct scheduler_node final {
	// Construct a node from a system.
	// The system can not be null
	scheduler_node(detail::system_base* _sys) : sys(_sys), dependents{}, unfinished_dependencies{0}, dependencies{0} {
		Pre(sys != nullptr);
	}

	scheduler_node(scheduler_node const& other) {
		sys = other.sys;
		dependents = other.dependents;
		dependencies = other.dependencies;
		unfinished_dependencies = other.unfinished_dependencies.load();
	}

	scheduler_node& operator=(scheduler_node const& other) {
		sys = other.sys;
		dependents = other.dependents;
		dependencies = other.dependencies;
		unfinished_dependencies = other.unfinished_dependencies.load();
		return *this;
	}

	detail::system_base* get_system() const noexcept {
		return sys;
	}

	// Add a dependant to this system. This system has to run to
	// completion before the dependents can run.
	void add_dependent(size_t node_index) {
		dependents.push_back(node_index);
	}

	// Increase the dependency counter of this system. These dependencies has to
	// run to completion before this system can run.
	void increase_dependency_count() {
		Pre(dependencies != std::numeric_limits<int16_t>::max());
		dependencies += 1;
	}

	// Resets the unfinished dependencies to the total number of dependencies.
	void reset_unfinished_dependencies() {
		unfinished_dependencies = dependencies;
	}

	// Called from systems we depend on when they have run to completion.
	void dependency_done() {
		unfinished_dependencies.fetch_sub(1, std::memory_order_release);
	}

	void run(std::vector<struct scheduler_node>& nodes) {
		// If we are not the last node here, leave
		if (unfinished_dependencies.load(std::memory_order_acquire) != 0)
			return;

		// Run the system
		sys->run();

		// Notify the dependents that we are done
		for (size_t const node : dependents)
			nodes[node].dependency_done();

		// Run the dependents in parallel
		std::for_each(std::execution::par, dependents.begin(), dependents.end(), [&nodes](size_t node) {
			nodes[node].run(nodes);
		});
	}

private:
	// The system to execute
	detail::system_base* sys{};

	// The systems that depend on this
	std::vector<size_t> dependents{};

	// The number of systems this depends on
	std::atomic<int> unfinished_dependencies = 0;
	int dependencies = 0;
};

// Schedules systems for concurrent execution based on their components.
class scheduler final {
	// A group of systems with the same group id
	struct systems_group final {
		std::vector<scheduler_node> all_nodes;
		std::vector<std::size_t> entry_nodes{};
		int id;

		systems_group() {}
		systems_group(int group_id) : id(group_id) {}

		// Runs the entry nodes in parallel
		void run() {
			std::for_each(std::execution::par, entry_nodes.begin(), entry_nodes.end(), [this](size_t node_id) {
				all_nodes[node_id].run(all_nodes);
			});
		}
	};

	std::vector<systems_group> groups;

protected:
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
		return *groups.insert(insert_point, systems_group{id});
	}

public:
	scheduler() {}

	void insert(detail::system_base* sys) {
		// Find the group
		auto& group = find_group(sys->get_group());

		// Create a new node with the system
		size_t const node_index = group.all_nodes.size();
		scheduler_node& node = group.all_nodes.emplace_back(sys);

		// Find a dependant system for each component
		bool inserted = false;
		auto const end = group.all_nodes.rend();
		for (auto const hash : sys->get_type_hashes()) {
			auto it = std::next(group.all_nodes.rbegin()); // 'next' to skip the newly added system
			while (it != end) {
				scheduler_node& dep_node = *it;
				// If the other system doesn't touch the same component,
				// then there can be no dependecy
				if (dep_node.get_system()->has_component(hash)) {
					if (dep_node.get_system()->writes_to_component(hash) || sys->writes_to_component(hash)) {
						// The system writes to the component,
						// so there is a strong dependency here.
						inserted = true;
						dep_node.add_dependent(node_index);
						node.increase_dependency_count();
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
			group.entry_nodes.push_back(node_index);
		}
	}

	// Clears all the schedulers data
	void clear() {
		groups.clear();
	}

	void run() {
		// Reset the execution data
		for (auto& group : groups) {
			for (auto& node : group.all_nodes)
				node.reset_unfinished_dependencies();
		}

		// Run the groups in succession
		for (auto& group : groups) {
			group.run();
		}
	}
};

} // namespace ecs::detail

#endif // !ECS_SYSTEM_SCHEDULER

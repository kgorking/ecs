#ifndef __SYSTEM_SCHEDULER
#define __SYSTEM_SCHEDULER

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
        scheduler_node(system_base* sys)
            : sys(sys)
            , dependants{}
            , dependencies{0}
            , unfinished_dependencies{0} {
            Expects(sys != nullptr);
        }

        scheduler_node(scheduler_node const& other) {
            sys = other.sys;
            dependants = other.dependants;
            dependencies = other.dependencies;
            unfinished_dependencies = other.unfinished_dependencies.load();
        }

        system_base* get_system() const noexcept {
            return sys;
        }

        // Add a dependant to this system. This system has to run to
        // completion before the dependants can run.
        void add_dependant(size_t node_index) {
            dependants.push_back(node_index);
        }

        // Increase the dependency counter of this system. These dependencies has to
        // run to completion before this system can run.
        void increase_dependency_count() {
            Expects(dependencies != std::numeric_limits<uint16_t>::max()); // You have 32k dependencies on a single
                                                                           // system. Just delete your code.
            dependencies += 1;
        }

        // Resets the unfinished dependencies to the total number of dependencies.
        void reset_unfinished_dependencies() {
            unfinished_dependencies = dependencies;
        }

        // Called from systems we depend on when they have run to completion.
        void dependency_done() {
            unfinished_dependencies.fetch_sub(1, std::memory_order_acquire);
        }

        void run(std::vector<struct scheduler_node>& nodes) {
            if (unfinished_dependencies.load(std::memory_order_release) > 0) {
                return;
            }

            sys->update();

            std::for_each(std::execution::par, dependants.begin(), dependants.end(), [&nodes](auto node) {
                nodes[node].dependency_done();
                nodes[node].run(nodes);
            });
        }

        scheduler_node& operator=(scheduler_node const& other) {
            sys = other.sys;
            dependants = other.dependants;
            dependencies = other.dependencies;
            unfinished_dependencies = other.unfinished_dependencies.load();
        }

    private:
        // The system to execute
        system_base* sys{};

        // The systems that depend on this
        std::vector<size_t> dependants{};

        // The number of systems this depends on
        uint16_t dependencies = 0;
        std::atomic<uint16_t> unfinished_dependencies = 0;
    };

    // Schedules systems for concurrent execution based on their components.
    class scheduler final {
        // A group of systems with the same group id
        struct group final {
            int id;
            std::vector<scheduler_node> all_nodes;
            std::vector<std::size_t> entry_nodes{};

            void run(size_t node_index) {
                all_nodes[node_index].run(all_nodes);
            }
        };

        std::vector<group> groups;

    protected:
        group& find_group(int id) {
            // Look for an existing group
            if (!groups.empty()) {
                for (auto& group : groups) {
                    if (group.id == id) {
                        return group;
                    }
                }
            }

            // No group found, so find an insertion point
            auto const insert_point =
                std::upper_bound(groups.begin(), groups.end(), id, [](int id, group const& sg) { return id < sg.id; });

            // Insert the group and return it
            return *groups.insert(insert_point, group{id, {}, {}});
        }

    public:
        void insert(system_base* sys) {
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
                            dep_node.add_dependant(node_index);
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

        void run() {
            // Reset the execution data
            for (auto& group : groups) {
                for (auto& node : group.all_nodes)
                    node.reset_unfinished_dependencies();
            }

            // Run the groups in succession
            for (auto& group : groups) {
                std::for_each(std::execution::par, group.entry_nodes.begin(), group.entry_nodes.end(),
                    [&group](auto node) { group.run(node); });
            }
        }
    };
} // namespace ecs::detail

#endif // !__SYSTEM_SCHEDULER

#ifndef __SYSTEM_SCHEDULER
#define __SYSTEM_SCHEDULER

#include <vector>
#include <execution>
#include <algorithm>
#include <mutex>

#include "system_base.h"
#include "contract.h"

namespace ecs::detail {
    // Describes a node in the scheduler execution graph
    struct scheduler_node {
        // Construct a node from a system.
        // The system can not be null
        scheduler_node(system_base *sys) : sys(sys) {
            Expects(sys != nullptr);
        }

        system_base* get_system() const noexcept {
            return sys;
        }

        // Add a child to this node.
        void add_child(size_t node_index) {
            children.push_back(node_index);
        }

        void increase_parent_count() {
            total_parents += 1;
        }

        void reset_run() {
            unfinished_parents = total_parents;
        }

        void run(std::vector<struct scheduler_node> & nodes) {
            static std::mutex run_mutex;
            {
                std::scoped_lock sl(run_mutex);
                if(unfinished_parents > 0) {
                    unfinished_parents--;

                    if (unfinished_parents > 0) {
                        return;
                    }
                }
            }

            sys->update();

            std::for_each(std::execution::par, children.begin(), children.end(), [&nodes](auto node) {
                nodes[node].run(nodes);
            });
        }

    private:
        // The system to execute
        system_base * sys{};

        // The systems that depend on this
        std::vector<std::size_t> children{};

        // The number of systems this depends on
        int16_t total_parents = 0;
        int16_t unfinished_parents = 0;
    };

    // Schedules systems for concurrent execution based on their components.
    class scheduler {
        // A group of systems with the same group id
        struct group {
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
                for (auto &group : groups) {
                    if (group.id == id) {
                        return group;
                    }
                }
            }

            // No group found, so find an insertion point
            auto const insert_point = std::upper_bound(groups.begin(), groups.end(), id, [](int id, group const& sg) {
                return id < sg.id;
            });

            // Insert the group and return it
            return *groups.insert(insert_point, group{id, {}, {}});
        }

    public:
        void insert(system_base * sys) {
            // Find the group
            auto & group = find_group(sys->get_group());

            // Create a new node with the system
            size_t const node_index = group.all_nodes.size();
            scheduler_node & node = group.all_nodes.emplace_back(sys);

            // Find a dependant system for each component
            bool inserted = false;
            auto const end = group.all_nodes.rend();
            for (auto const hash : sys->get_type_hashes()) {
                auto it = std::next(group.all_nodes.rbegin());
                while(it != end) {
                    scheduler_node & dep_node = *it;
                    // If the other system doesn't touch the same component,
                    // then there can be no dependecy
                    if (dep_node.get_system()->has_component(hash)) {
                        if (dep_node.get_system()->writes_to_component(hash) || sys->writes_to_component(hash)) {
                            // The system writes to the component,
                            // so there is a strong dependency here.
                            // Order is preserved.
                            inserted = true;
                            dep_node.add_child(node_index);
                            node.increase_parent_count();
                            break;
                        }
                        else { // 'other' reads component
                            // These systems have a weak read/read dependency
                            // and can be scheduled concurrently
                            // Order does not need to be preserved.
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
            for (auto & group : groups) {
                for (auto & node : group.all_nodes)
                    node.reset_run();
            }

            // Run the groups in succession
            for (auto & group : groups) {
                std::for_each(std::execution::par, group.entry_nodes.begin(), group.entry_nodes.end(), [&group](auto node) {
                    group.run(node);
                });
            }
        }
    };
}

#endif // !__SYSTEM_SCHEDULER

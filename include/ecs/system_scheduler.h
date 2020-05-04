#ifndef __SYSTEM_SCHEDULER
#define __SYSTEM_SCHEDULER

#include <list>
#include <vector>
#include <execution>
#include <algorithm>
#include <mutex>

#include "system_base.h"
#include "contract.h"

namespace ecs::detail {
    // Describes a point in the scheduler execution graph
    struct scheduler_node {
        // Construct a node from a system.
        // The system can not be null
        scheduler_node(system_base *sys) : sys(sys) {
            Expects(sys != nullptr);
        }

        system_base* get_system() const noexcept {
            return sys;
        }

        // Add a dependency to this node.
        // The dependency can not be null
        void add_dependant(scheduler_node* node) {
            Expects(node != nullptr);
            dependants.push_back(node);
            node->total_dependencies++;
        }

        void reset_run() {
            remaining_dependencies = total_dependencies;
        }

        void run() {
            static std::mutex run_mutex; {
                std::scoped_lock sl(run_mutex);
                if(remaining_dependencies > 0) {
                    remaining_dependencies--;

                    if (remaining_dependencies > 0) {
                        return;
                    }
                }
            }

            sys->update();

            std::for_each(std::execution::par, dependants.begin(), dependants.end(), [](auto & node) {
                node->run();
            });
        }

    private:
        // The system to execute
        system_base * sys{};

        // The systems that depend on this
        std::vector<scheduler_node*> dependants{};

        // The number of systems this depends on
        int16_t total_dependencies = 0;
        int16_t remaining_dependencies = 0;
    };

    // Schedules systems based on their components for concurrent execution.
    class system_scheduler {
        // A group of systems with the same group id
        struct system_group {
            int id;
            std::list<scheduler_node> all_nodes;
            std::vector<scheduler_node*> entry_nodes{};
        };

        std::list<system_group> groups;

    protected:
        system_group* find_group(int id) {
            // Look for an existing group
            if (!groups.empty()) {
                for (auto &group : groups) {
                    if (group.id == id) {
                        return &group;
                    }
                }
            }

            // No group found, so find an insertion point
            auto const insert_point = std::upper_bound(groups.begin(), groups.end(), id, [](int id, system_group const& sg) {
                return id < sg.id;
            });

            // Insert the group and return it
            return &*groups.insert(insert_point, system_group{id, {}, {}});
        }

    public:
        void reset() {
            groups.clear();
        }

        void insert(system_base * sys) {
            // Find the group
            auto group = find_group(sys->get_group());

            // Create a new node with the system
            scheduler_node & node = group->all_nodes.emplace_back(sys);

            // Find a dependant system for each component
            bool inserted = false;
            auto const end = group->all_nodes.rend();
            for (auto const hash : sys->get_type_hashes()) {
                auto it = std::next(group->all_nodes.rbegin());
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
                            dep_node.add_dependant(&node);
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
                group->entry_nodes.push_back(&node);
            }
        }

        void run() {
            // Reset the execution data
            for (auto & group : groups) {
                for (auto & node : group.all_nodes)
                    node.reset_run();
            }

            // Run the groups in succession
            for (auto const& group : groups) {
                std::for_each(std::execution::par, group.entry_nodes.begin(), group.entry_nodes.end(), [](auto node) {
                    node->run();
                });
            }
        }
    };
}

#endif // !__SYSTEM_SCHEDULER

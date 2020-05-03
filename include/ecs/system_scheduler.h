#ifndef __SYSTEM_SCHEDULER
#define __SYSTEM_SCHEDULER

#include <iostream>
#include <string>

#include <list>
#include <vector>
#include <execution>
#include <algorithm>
#include <mutex>
#include "system_base.h"
#include "contract.h"

namespace ecs::detail {
    // Describes a single point of execution
    struct system_node {
        // Construct a node from a system which can not be null
        system_node(system_base *sys) : sys(sys) {
            //Expects(sys != nullptr);
        }

        system_base* get_system() const noexcept {
            return sys;
        }

        // Add a dependency to this node.
        // The dependency can not be null
        void add_dependant(system_node* node) {
            Expects(node != nullptr);
            dependants.push_back(node);
            node->total_dependencies++;
        }

        void reset_run() {
            remaining_dependencies = total_dependencies;
        }

        void run() {
            static std::mutex run_mutex, cout_mutex;
            if(remaining_dependencies > 0) {
                std::scoped_lock sl(run_mutex);
                remaining_dependencies--;

                if (remaining_dependencies > 0) {
                    std::scoped_lock sl2(cout_mutex);
                    std::cout << "skipping " << sys->get_signature() << " - " << remaining_dependencies << '\n';
                    return;
                }
            }

            if (sys != nullptr) {
                {
                    std::scoped_lock sl3(cout_mutex);
                    std::cout << "running " << sys->get_signature() << '\n';
                }
                sys->update();
            }

            std::for_each(std::execution::par, dependants.begin(), dependants.end(), [](auto & node) {
                node->run();
            });
        }

        void print(int indent) const {
            if (sys != nullptr) {
                std::string s(indent, ' ');
                std::cout << s << sys->get_signature() << '\n';
            }

            for (auto dep : dependants) {
                dep->print(1 + indent);
            }
        }

    private:
        // The system to execute
        system_base * sys{};

        // The systems that depend on this system
        std::vector<system_node*> dependants{};

        int16_t total_dependencies = 0;
        int16_t remaining_dependencies = 0;
    };

    // A class to order systems to be run async without causing dataraces.
    // Systems are ordered based on what components they read/write.
    //
    class system_scheduler {
        std::list<system_node> all_nodes; // all systems added, in linear order
        system_node entry_node{nullptr};

    public:
        void reset() {
            entry_node = system_node{nullptr};
            all_nodes.clear();
        }

        void insert(system_base * sys) {
            // Create a new node with the system
            system_node * node = &all_nodes.emplace_back(sys);

            auto const end = all_nodes.rend();

            bool inserted = false;

            // Find a dependant system for each component
            for (auto const hash : sys->get_type_hashes()) {
                auto it = std::next(all_nodes.rbegin());
                while(it != end) {
                    system_node & dep_node = *it;
                    // If the other system doesn't touch the same component,
                    // then there can be no dependecy
                    if (dep_node.get_system()->has_component(hash)) {
                        if (dep_node.get_system()->writes_to_component(hash) || sys->writes_to_component(hash)) {
                            // The system writes to the component,
                            // so there is a strong dependency here.
                            // Order is preserved.
                            inserted = true;
                            dep_node.add_dependant(node);
                            std::cout << sys->get_signature() << " -> " << dep_node.get_system()->get_signature() << '\n';
                            break;
                        }
                        else { // 'other' reads component
                            // These systems have a weak read/read dependency
                            // and can be scheduled concurrently
                            // Order does not need to be preserved.
                            //continue;
                        }
                    }

                    ++it;
                }
            }

            if (!inserted) {
                std::cout << "New entrypoint: " << sys->get_signature() << '\n';
                entry_node.add_dependant(node);
            }
        }

        void print_lanes() const {
            entry_node.print(0);
        }

        void run() {
            entry_node.reset_run();
            for (auto & node : all_nodes)
                node.reset_run();

            entry_node.run();
        }
    };
}

#endif // !__SYSTEM_SCHEDULER

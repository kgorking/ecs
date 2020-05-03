#ifndef __SYSTEM_SCHEDULER
#define __SYSTEM_SCHEDULER

#include <vector>
#include <execution>
#include "system_base.h"
#include <iostream>

namespace ecs::detail {
    // Describes a single point of execution
    struct execution_point {
        // The system to execute
        system_base * sys;

        // The total number of lanes that depend on this system
        uint8_t num_lanes;

        // The remaining number of lanes that depend on this system.
        // Reset to 'num_dependencies' at the start of each scheduler run.
        uint8_t remaining_lanes;

        std::vector<execution_point*> dependants;
    };

    // Contains systems that are dependent on each other
    using execution_lane = std::vector<int>;

    // A class to order systems to be run async without causing dataraces.
    // Systems are ordered based on what components they read/write.
    //
    class system_scheduler {
        std::vector<system_base*> systems; // all systems added, in linear order
        std::vector<execution_lane> lanes;
        std::vector<execution_point> points;

    public:
        void insert(system_base * sys) {
            int new_index = static_cast<int>(points.size());
            execution_point *new_ep = &points.emplace_back(execution_point {sys,0,0});

            if (!lanes.empty()) {
                // Blast through the lanes and check if any dependencies found
                std::vector<int> deps;
                int lane_count = 0;
                for (execution_lane & lane : lanes) {
                    deps.clear();
                    for (int const ep_index : lane) {
                        if (points[ep_index].sys->depends_on(sys)) {
                            std::cout << "* adding " << sys->get_signature() << " to lane " << lane_count << '\n';
                            deps.push_back(new_index);
                            new_ep->num_lanes += 1;

                            points[ep_index].dependants.push_back(new_ep);
                            break;
                        }
                    }

                    if (!deps.empty())
                        lane.insert(lane.end(), deps.begin(), deps.end());

                    lane_count++;
                }

                if (new_ep->num_lanes == 0) {
                    new_ep->num_lanes = 1;
                    std::cout << "creating lane " << lanes.size() << " with " << sys->get_signature() << '\n';
                    lanes.push_back({new_index});
                }
            }
            else {
                // No lanes exist, so create a new lane
                new_ep->num_lanes = 1;
                std::cout << "creating lane 0 with " << sys->get_signature() << '\n';
                lanes.push_back({new_index});
            }

            systems.push_back(sys);
        }

        void print_lanes() const {
            int lane_count = 0;
            for (auto const& lane : lanes) {
                std::cout << "lane " << lane_count << '\n';
                for(int ep_index : lane) {
                    std::cout << ' ' << points[ep_index].sys->get_signature() << '\n';
                }

                lane_count++;
            }
        }

        void run() {
            for (auto & ep : points) {
                ep.remaining_lanes = ep.num_lanes;
            }

            int lane_count = 0;
            std::for_each(lanes.begin(), lanes.end(), [this, &lane_count](auto & lane) {
                for (int ep_index : lane) {
                    std::cout << 'L' << lane_count << ' ' << points[ep_index].sys->get_signature();

                    if (points[ep_index].remaining_lanes == 1) {
                        points[ep_index].sys->update();
                        std::cout << " - executed\n";
                    }
                    else {
                        points[ep_index].remaining_lanes--;
                        std::cout << " - skipped\n";
                    }
                }

                lane_count++;
            });
        }
    };
}

#endif // !__SYSTEM_SCHEDULER

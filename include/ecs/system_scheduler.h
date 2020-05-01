#ifndef __SYSTEM_SCHEDULER
#define __SYSTEM_SCHEDULER

#include <vector>
#include "system_base.h"

namespace ecs::detail {
    // Contains systems that are dependent on each other
    using execution_lane = std::vector<system_base*>;

    // A class to order systems to be run async without causing dataraces.
    // Systems are ordered based on what components they read/write.
    //
    class system_scheduler {
        std::vector<execution_lane> lanes;

    public:
        void insert(system_base & sys) {
            if (!lanes.empty()) {
                // Blast through the lanes and check if any dependencies found
                for (auto & lane : lanes) {
                    for (auto * lane_sys : lane) {
                        if (lane_sys->depends_on(sys)) {
                            lane.push_back(&sys);
                            return;
                        }
                    }
                }
            }

            // No dependencies found, so create a new lane
            lanes.emplace_back(execution_lane{&sys});
        }
    };
}

#endif // !__SYSTEM_SCHEDULER

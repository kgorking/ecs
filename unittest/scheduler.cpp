#include "catch.hpp"
#include <ecs/ecs.h>
#include <atomic>

// Tests to make sure the scheduler works as intended.
TEST_CASE("Scheduler") {

    SECTION("lots of dependencies") {
        struct sched_test {};

        // Create 100 systems that will execute concurrently
        std::atomic_int lambda_counter = 0;
        auto const lambda = [&lambda_counter](sched_test const&) {
            ++lambda_counter;
        };
        for (int i=0; i<100; i++) {
            ecs::make_system(lambda);
        }

        // Create a system that should only run after the 100 systems,
        // because it has 100 dependencies
        auto const dependant = [&lambda_counter](sched_test&) {
            CHECK(lambda_counter == 100);
        };
        ecs::make_system(dependant);

        ecs::add_component(0, sched_test{});
        ecs::commit_changes();

        ecs::run_systems();
    }
}

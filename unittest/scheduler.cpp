#include "catch.hpp"
#include <atomic>
#include <ecs/ecs.h>
#include <thread>

using namespace std::chrono_literals;

template<size_t I>
struct type {};

// Tests to make sure the scheduler works as intended.
TEST_CASE("Scheduler") {

    SECTION("lots of dependencies") {
        ecs::detail::get_context().reset();

        struct sched_test {};

        // Create 100 systems that will execute concurrently
        std::atomic_int lambda_counter = 0;
        auto const lambda = [&lambda_counter](sched_test const&) { ++lambda_counter; };
        for (int i = 0; i < 100; i++) {
            ecs::make_system(lambda);
        }

        // Create a system that should only run after the 100 systems,
        // because it has 100 dependencies
        auto const dependant = [&lambda_counter](sched_test&) { CHECK(lambda_counter == 100); };
        ecs::make_system(dependant);

        ecs::add_component(0, sched_test{});
        ecs::commit_changes();

        ecs::run_systems();
    }

    SECTION("Correct concurrency") {
        ecs::detail::get_context().reset();

        std::atomic_bool sys1 = false;
        std::atomic_bool sys2 = false;
        std::atomic_bool sys3 = false;
        std::atomic_bool sys4 = false;
        std::atomic_bool sys5 = false;
        std::atomic_bool sys6 = false;

        ecs::make_system([&sys1](type<0>&, type<1> const&) {
            sys1 = true;
        });

        ecs::make_system([&sys2, &sys1](type<1>&) {
            CHECK(sys1 == true);
            sys2 = true;
        });

        ecs::make_system([&sys3](type<2>&) {
            std::this_thread::sleep_for(20ms);
            sys3 = true;
        });

        ecs::make_system([&sys4, &sys1, &sys3](type<0> const&) {
            CHECK(sys3 == false);
            CHECK(sys1 == true);
            sys4 = true;
        });

        ecs::make_system([&sys5, &sys3, &sys1](type<2>&, type<0> const&) {
            CHECK(sys3 == true);
            CHECK(sys1 == true);
            sys5 = true;
        });

        ecs::make_system([&sys6, &sys5](type<2> const&) {
            CHECK(sys5 == true);
            sys6 = true;
        });

        ecs::add_component(0, type<0>{}, type<1>{}, type<2>{});
        ecs::update_systems();

        CHECK(sys1 == true);
        CHECK(sys2 == true);
        CHECK(sys3 == true);
        CHECK(sys4 == true);
        CHECK(sys5 == true);
        CHECK(sys6 == true);
    }
}

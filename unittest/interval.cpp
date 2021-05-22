#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>
#include <chrono>
#include <array>

using namespace std::chrono_literals;

TEST_CASE("System frequency") {
    ecs::runtime ecs;

    constexpr int num_frequencies = 8;
    constexpr int frequencies[num_frequencies] = {2, 5, 10, 17, 345, 1'619, 33'333, 450'000};

    std::array < int, num_frequencies> counters;
    counters.fill(0);

    ecs.make_system<ecs::opts::interval<1000.0 / frequencies[0]>>([&counters](int const&) { ++counters[0]; });
    ecs.make_system<ecs::opts::interval<1000.0 / frequencies[1]>>([&counters](int const&) { ++counters[1]; });
    ecs.make_system<ecs::opts::interval<1000.0 / frequencies[2]>>([&counters](int const&) { ++counters[2]; });
    ecs.make_system<ecs::opts::interval<1000.0 / frequencies[3]>>([&counters](int const&) { ++counters[3]; });
    ecs.make_system<ecs::opts::interval<1000.0 / frequencies[4]>>([&counters](int const&) { ++counters[4]; });
    ecs.make_system<ecs::opts::interval<1000.0 / frequencies[5]>>([&counters](int const&) { ++counters[5]; });
    ecs.make_system<ecs::opts::interval<1000.0 / frequencies[6]>>([&counters](int const&) { ++counters[6]; });
    ecs.make_system<ecs::opts::interval<1000.0 / frequencies[7]>>([&counters](int const&) { ++counters[7]; });

    ecs.add_component({0, 0}, int{});
    ecs.commit_changes();

    // Run the systems for 1 second
    auto const start = std::chrono::high_resolution_clock::now();
    while (std::chrono::high_resolution_clock::now() - start <= 1s)
        ecs.run_systems();

    // Check that systems with frequencies above zero do not run 
    // more times than the frequency
    for (int i = 0; i < num_frequencies; i++) {
        CHECK(counters[i] <= frequencies[i]);
    }
}

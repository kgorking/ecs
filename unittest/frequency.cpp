#include "catch.hpp"
#include <ecs/ecs.h>

TEST_CASE("System frequency") {
    ecs::detail::get_context().reset();

    constexpr int num_frequencies = 7;
    constexpr int frequencies[num_frequencies] = {2, 5, 10, 17, 345, 1'619, 33'333};

    int counters[num_frequencies];
    std::fill_n(counters, num_frequencies, 0);

    ecs::make_system<ecs::opts::frequency<frequencies[0]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[0]; });
    ecs::make_system<ecs::opts::frequency<frequencies[1]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[1]; });
    ecs::make_system<ecs::opts::frequency<frequencies[2]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[2]; });
    ecs::make_system<ecs::opts::frequency<frequencies[3]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[3]; });
    ecs::make_system<ecs::opts::frequency<frequencies[4]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[4]; });
    ecs::make_system<ecs::opts::frequency<frequencies[5]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[5]; });
    ecs::make_system<ecs::opts::frequency<frequencies[6]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[6]; });

    ecs::add_component({0, 0}, int{});
    ecs::commit_changes();

    // Run the systems for 1 second
    using namespace std::chrono_literals;
    auto const start = std::chrono::high_resolution_clock::now();
    while (std::chrono::high_resolution_clock::now() - start < 1s)
        ecs::run_systems();

    // Check that systems with frequencies above zero do not run 
    // more times than the frequency
    for (int i = 0; i < num_frequencies; i++) {
        CHECK(counters[i] <= frequencies[i]);
    }
}

#include <ecs/ecs.h>
#include <array>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

constexpr int num_intervals = 9;

int main() {
    using ecs::opts::interval;

    std::array<int, num_intervals> counters;
    counters.fill(0);

    //ecs::make_system<interval<0s>>([&counters](int const&) { ++counters[0]; });
    /*ecs::make_system<frequency<frequencies[1]>>([&counters](int const&) { ++counters[1]; });
    ecs::make_system<frequency<frequencies[2]>>([&counters](int const&) { ++counters[2]; });
    ecs::make_system<frequency<frequencies[3]>>([&counters](int const&) { ++counters[3]; });
    ecs::make_system<frequency<frequencies[4]>>([&counters](int const&) { ++counters[4]; });
    ecs::make_system<frequency<frequencies[5]>>([&counters](int const&) { ++counters[5]; });
    ecs::make_system<frequency<frequencies[6]>>([&counters](int const&) { ++counters[6]; });
    ecs::make_system<frequency<frequencies[7]>>([&counters](int const&) { ++counters[7]; });
    ecs::make_system<frequency<frequencies[8]>>([&counters](int const&) { ++counters[8]; });*/

    ecs::add_component({0, 0}, int{});
    ecs::commit_changes();

    // Run the systems for 1 second
    auto const start = std::chrono::high_resolution_clock::now();
    while (std::chrono::high_resolution_clock::now() - start <= 1s)
        ecs::run_systems();

    for (int i = 0; i < num_intervals; i++) {
        std::cout << "System updated " << counters[i] << " times\n";
    }
}

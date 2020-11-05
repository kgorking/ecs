#include <ecs/ecs.h>
#include <array>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

constexpr int num_frequencies = 9;
using freq_arr = std::array<int, num_frequencies>;

int main() {
    constexpr freq_arr frequencies{0, 2, 5, 10, 17, 88, 1619, 33333, 450000};
    freq_arr counters;
    counters.fill(0);

    ecs::make_system<ecs::opts::frequency<frequencies[0]>>([&counters](int const&) { ++counters[0]; });
    ecs::make_system<ecs::opts::frequency<frequencies[1]>>([&counters](int const&) { ++counters[1]; });
    ecs::make_system<ecs::opts::frequency<frequencies[2]>>([&counters](int const&) { ++counters[2]; });
    ecs::make_system<ecs::opts::frequency<frequencies[3]>>([&counters](int const&) { ++counters[3]; });
    ecs::make_system<ecs::opts::frequency<frequencies[4]>>([&counters](int const&) { ++counters[4]; });
    ecs::make_system<ecs::opts::frequency<frequencies[5]>>([&counters](int const&) { ++counters[5]; });
    ecs::make_system<ecs::opts::frequency<frequencies[6]>>([&counters](int const&) { ++counters[6]; });
    ecs::make_system<ecs::opts::frequency<frequencies[7]>>([&counters](int const&) { ++counters[7]; });
    ecs::make_system<ecs::opts::frequency<frequencies[8]>>([&counters](int const&) { ++counters[8]; });

    ecs::add_component({0, 0}, int{});
    ecs::commit_changes();

    // Run the systems for 1 second
    auto const start = std::chrono::high_resolution_clock::now();
    while (std::chrono::high_resolution_clock::now() - start <= 1s)
        ecs::run_systems();

    for (int i = 0; i < num_frequencies; i++) {
        std::cout << "System with " << frequencies[i] << "hz frequency updated " << counters[i] << " times\n";
    }
}

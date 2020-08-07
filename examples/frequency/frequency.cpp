#include <ecs/ecs.h>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

int main() {
    constexpr int num_frequencies = 9;
    constexpr size_t frequencies[num_frequencies] = {0, 2, 5, 10, 17, 88, 1619, 33333, 450000};

    int counters[num_frequencies];
    std::fill_n(counters, num_frequencies, 0);

    ecs::make_system<ecs::opts::frequency<frequencies[0]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[0]; });
    ecs::make_system<ecs::opts::frequency<frequencies[1]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[1]; });
    ecs::make_system<ecs::opts::frequency<frequencies[2]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[2]; });
    ecs::make_system<ecs::opts::frequency<frequencies[3]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[3]; });
    ecs::make_system<ecs::opts::frequency<frequencies[4]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[4]; });
    ecs::make_system<ecs::opts::frequency<frequencies[5]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[5]; });
    ecs::make_system<ecs::opts::frequency<frequencies[6]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[6]; });
    ecs::make_system<ecs::opts::frequency<frequencies[7]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[7]; });
    ecs::make_system<ecs::opts::frequency<frequencies[8]>, ecs::opts::not_parallel>([&counters](int const&) { ++counters[8]; });

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

#include <ecs/ecs.h>
#include <array>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

constexpr int num_intervals = 9;
using interval_arr = std::array<double, num_intervals>;

int main() {
    constexpr interval_arr intervals{0, 2, 5, 10, 17, 88, 1619, 33333, 450000};

    using ecs::opts::interval;

    std::array<int, num_intervals> counters;
    counters.fill(0);

    ecs::runtime ecs;
    ecs.make_system<interval<       intervals[0]>>([&counters](int const&) { ++counters[0]; });
    ecs.make_system<interval<1000.0/intervals[1]>>([&counters](int const&) { ++counters[1]; });
    ecs.make_system<interval<1000.0/intervals[2]>>([&counters](int const&) { ++counters[2]; });
    ecs.make_system<interval<1000.0/intervals[3]>>([&counters](int const&) { ++counters[3]; });
    ecs.make_system<interval<1000.0/intervals[4]>>([&counters](int const&) { ++counters[4]; });
    ecs.make_system<interval<1000.0/intervals[5]>>([&counters](int const&) { ++counters[5]; });
    ecs.make_system<interval<1000.0/intervals[6]>>([&counters](int const&) { ++counters[6]; });
    ecs.make_system<interval<1000.0/intervals[7]>>([&counters](int const&) { ++counters[7]; });
    ecs.make_system<interval<1000.0/intervals[8]>>([&counters](int const&) { ++counters[8]; });

    ecs.add_component({0, 0}, int{});
    ecs.commit_changes();

    // Run the systems for 1 second
    auto const start = std::chrono::high_resolution_clock::now();
    while (std::chrono::high_resolution_clock::now() - start <= 1s)
        ecs.run_systems();

    for (int i = 0; i < num_intervals; i++) {
        std::cout << "System updated " << counters[i] << " times\n";
    }
}

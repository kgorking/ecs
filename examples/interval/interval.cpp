#include <array>
#include <chrono>
#include <ecs/ecs.h>
#include <iostream>

using namespace std::chrono_literals;

int main() {
	constexpr size_t num_intervals = 10;
	constexpr int intervals[num_intervals] = {500, 200, 100, 50, 25, 15, 5, 2, 750, 250};

	using ecs::opts::interval;

	std::array<int, num_intervals> counters;
	counters.fill(0);

	ecs::runtime rt;
	rt.make_system<interval<intervals[0], 0>>([&counters](int const &) { ++counters[0]; });
	rt.make_system<interval<intervals[1], 0>>([&counters](int const &) { ++counters[1]; });
	rt.make_system<interval<intervals[2], 0>>([&counters](int const &) { ++counters[2]; });
	rt.make_system<interval<intervals[3], 0>>([&counters](int const &) { ++counters[3]; });
	rt.make_system<interval<intervals[4], 0>>([&counters](int const &) { ++counters[4]; });
	rt.make_system<interval<intervals[5], 0>>([&counters](int const &) { ++counters[5]; });
	rt.make_system<interval<intervals[6], 0>>([&counters](int const &) { ++counters[6]; });
	rt.make_system<interval<intervals[7], 0>>([&counters](int const &) { ++counters[7]; });
	rt.make_system<interval<0, intervals[8]>>([&counters](int const &) { ++counters[8]; });
	rt.make_system<interval<0, intervals[9]>>([&counters](int const &) { ++counters[9]; });

	rt.add_component({0, 0}, int{});
	rt.commit_changes();

	// Run the systems for 1 second
	auto const start = std::chrono::high_resolution_clock::now();
	while (std::chrono::high_resolution_clock::now() - start <= 1s)
		rt.run_systems();

	for (size_t i = 0; i < num_intervals - 2; i++) {
		std::cout << "System updated " << counters[i] << " times, maximum is " << 1'000 / intervals[i] << "\n";
	}
	for (size_t i = num_intervals - 2; i < num_intervals; i++) {
		std::cout << "System updated " << counters[i] << " times, maximum is " << 1'000'000 / intervals[i] << "\n";
	}
}

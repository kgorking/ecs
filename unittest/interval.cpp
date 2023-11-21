#include <ecs/ecs.h>
#include <array>
#include <chrono>
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace std::chrono_literals;

TEST_CASE("System frequency") {
	ecs::runtime rt;

	constexpr int num_intervals = 8;
	constexpr int intervals[num_intervals] = {500, 200, 100, 50, 250, 150, 50, 20};

	std::array<int, num_intervals> counters;
	counters.fill(0);

	using ecs::opts::interval;
	rt.make_system<interval<intervals[0], 0>>([&](int) { ++counters[0]; });
	rt.make_system<interval<intervals[1], 0>>([&](int) { ++counters[1]; });
	rt.make_system<interval<intervals[2], 0>>([&](int) { ++counters[2]; });
	rt.make_system<interval<intervals[3], 0>>([&](int) { ++counters[3]; });
	rt.make_system<interval<0, intervals[4]>>([&](int) { ++counters[4]; });
	rt.make_system<interval<0, intervals[5]>>([&](int) { ++counters[5]; });
	rt.make_system<interval<0, intervals[6]>>([&](int) { ++counters[6]; });
	rt.make_system<interval<0, intervals[7]>>([&](int) { ++counters[7]; });

	rt.add_component({0, 0}, int{});
	rt.commit_changes();

	// Run the systems for 1 second
	auto const start = std::chrono::high_resolution_clock::now();
	while (std::chrono::high_resolution_clock::now() - start <= 1s)
		rt.run_systems();

	// Verify the counter
	for (size_t i = 0; i < 4; i++) {
		CHECK(counters[i] > 0);
		CHECK(counters[i] <= 1'000 / intervals[i]);
	}
	for (size_t i = 4; i < num_intervals; i++) {
		CHECK(counters[i] > 0);
		CHECK(counters[i] <= 1'000'000 / intervals[i]);
	}
}

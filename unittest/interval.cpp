#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <array>
#include <chrono>
#include <ecs/ecs.h>

using namespace std::chrono_literals;

TEST_CASE("System frequency") {
	ecs::runtime ecs;

	constexpr int num_intervals = 8;
	constexpr int intervals[num_intervals] = {500, 200, 100, 50, 250, 150, 50, 20};

	std::array<int, num_intervals> counters;
	counters.fill(0);

	ecs.make_system<ecs::opts::interval<intervals[0], 0>>([&counters](int const&) { ++counters[0]; });
	ecs.make_system<ecs::opts::interval<intervals[1], 0>>([&counters](int const&) { ++counters[1]; });
	ecs.make_system<ecs::opts::interval<intervals[2], 0>>([&counters](int const&) { ++counters[2]; });
	ecs.make_system<ecs::opts::interval<intervals[3], 0>>([&counters](int const&) { ++counters[3]; });
	ecs.make_system<ecs::opts::interval<0, intervals[4]>>([&counters](int const&) { ++counters[4]; });
	ecs.make_system<ecs::opts::interval<0, intervals[5]>>([&counters](int const&) { ++counters[5]; });
	ecs.make_system<ecs::opts::interval<0, intervals[6]>>([&counters](int const&) { ++counters[6]; });
	ecs.make_system<ecs::opts::interval<0, intervals[7]>>([&counters](int const&) { ++counters[7]; });

	ecs.add_component({0, 0}, int{});
	ecs.commit_changes();

	// Run the systems for 1 second
	auto const start = std::chrono::high_resolution_clock::now();
	while (std::chrono::high_resolution_clock::now() - start <= 1s)
		ecs.run_systems();

	// Verify the counter
	for (size_t i = 0; i < 4; i++) {
		CHECK(counters[i] <= 1'000 / intervals[i]);
	}
	for (size_t i = 4; i < num_intervals; i++) {
		CHECK(counters[i] <= 1'000'000 / intervals[i]);
	}
}

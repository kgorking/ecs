// Benchmarks for sorted systems

#include <ecs/ecs.h>
#include <numeric>
#include <random>
#include <span>
#include "gbench/include/benchmark/benchmark.h"
#include "global.h"

static void build_sorted(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	std::vector<int> ints(static_cast<std::size_t>(1 + nentities));
	std::iota(ints.begin(), ints.end(), 0);

	std::random_device rd;
	std::mt19937 gen{rd()};
	std::shuffle(ints.begin(), ints.end(), gen);

	ecs::runtime rt;
	rt.add_component_span({0, nentities}, ints);
	rt.commit_changes();

	auto& sys = rt.make_system<ecs::opts::manual_update>([](int const&) {}, std::less<int>());
	for ([[maybe_unused]] auto const _ : state) {
		// triggers a rebuild
		sys.set_enable(true);
	}
}
ECS_BENCHMARK(build_sorted);

static void build_sorted_many_ranges(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	std::vector<int> ints(static_cast<std::size_t>(nentities));
	std::iota(ints.begin(), ints.end(), 0);

	std::random_device rd;
	std::mt19937 gen{rd()};
	std::shuffle(ints.begin(), ints.end(), gen);

	std::span const span{ints};

	ecs::runtime rt;
	// ranges span 8 components
	for(int i=0; i<nentities; i += 8) {
		rt.add_component_span({i, i+7}, span.subspan(i, 8));
		rt.commit_changes();
	}

	auto& sys = rt.make_system<ecs::opts::manual_update>([](int const&) {}, std::less<int>());
	for ([[maybe_unused]] auto const _ : state) {
		// triggers a rebuild
		sys.set_enable(true);
	}

	//state.SetItemsProcessed(nentities * state.iterations());
}
ECS_BENCHMARK(build_sorted_many_ranges);

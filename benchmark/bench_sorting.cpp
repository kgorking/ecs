// Benchmarks for sorted systems

#include "gbench/include/benchmark/benchmark.h"
#include "global.h"
#include <ecs/ecs.h>
#include <random>
#include <span>

static void build_sorted(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	std::vector<unsigned> ints(static_cast<std::size_t>(nentities));
	std::iota(ints.begin(), ints.end(), 0);

	std::random_device rd;
	std::mt19937 gen{rd()};
	std::shuffle(ints.begin(), ints.end(), gen);

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;

		state.BeginIgnoreTiming();
		ecs.add_component_span({1, nentities}, ints);
		ecs.commit_changes();
		state.EndIgnoreTiming();

		ecs.make_system([](int const&) {}, std::less<int>());
	}

	state.SetItemsProcessed(nentities * state.iterations());
}
ECS_BENCHMARK(build_sorted);

static void build_sorted_many_ranges(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	std::vector<unsigned> ints(static_cast<std::size_t>(nentities));
	std::iota(ints.begin(), ints.end(), 0);

	std::random_device rd;
	std::mt19937 gen{rd()};
	std::shuffle(ints.begin(), ints.end(), gen);

	std::span const span{ints};

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;

		state.BeginIgnoreTiming();
		// ranges span 8 components
		for(int i=1; i<=nentities / 8; i += 8) {
			ecs.add_component_span({i, i+7}, span.subspan(i, 8));
			ecs.commit_changes();
		}
		state.EndIgnoreTiming();

		ecs.make_system([](int const&) {}, std::less<int>());
	}

	state.SetItemsProcessed(nentities * state.iterations());
}
ECS_BENCHMARK(build_sorted_many_ranges);

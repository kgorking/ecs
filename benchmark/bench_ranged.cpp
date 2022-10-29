#include "gbench/include/benchmark/benchmark.h"
#include <complex>
#include <ecs/ecs.h>

#include "global.h"

void build_ranged(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::runtime ecs;
	ecs.make_system([](int) {});

	for ([[maybe_unused]] auto const _ : state) {
		ecs.add_component({0, nentities}, int{});
		ecs.commit_changes();

		state.BeginIgnoreTiming();
		ecs.remove_component<int>({0, nentities});
		ecs.commit_changes();
		state.EndIgnoreTiming();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(build_ranged);

void build_many_ranged(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::runtime ecs;
	ecs.make_system([](int) {});

	for ([[maybe_unused]] auto const _ : state) {
		for(int i=0; i<nentities / 8; i += 8) {
			ecs.add_component({i, i + 7}, int{});
			ecs.commit_changes();
		}

		state.BeginIgnoreTiming();
		ecs.remove_component<int>({0, nentities});
		ecs.commit_changes();
		state.EndIgnoreTiming();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(build_many_ranged);

void run_serial_ranged(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::runtime ecs;
	ecs.make_system<ecs::opts::not_parallel>(benchmark_system);

	ecs.add_component({0, nentities}, int{});
	ecs.commit_changes();

	for ([[maybe_unused]] auto const _ : state) {
		ecs.run_systems();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(run_serial_ranged);

void run_parallel_ranged(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::runtime ecs;
	ecs.make_system(benchmark_system);

	ecs.add_component({0, nentities}, int{});
	ecs.commit_changes();

	for ([[maybe_unused]] auto const _ : state) {
		ecs.run_systems();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(run_parallel_ranged);

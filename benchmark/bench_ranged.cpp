#include "gbench/include/benchmark/benchmark.h"
#include <complex>
#include <ecs/ecs.h>

#include "global.h"

void build_ranged_with_components(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;

		state.BeginIgnoreTiming();
		ecs.add_component({0, nentities}, int{});
		ecs.commit_changes();
		state.EndIgnoreTiming();

		ecs.make_system([](int) {});
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(build_ranged_with_components);

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

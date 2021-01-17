#include "gbench/include/benchmark/benchmark.h"
#include <complex>
#include <ecs/ecs.h>

#include "global.h"

void build_ranged_no_components(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();
		ecs::make([](int) {});
	}

	state.SetItemsProcessed(state.iterations());
}
ECS_BENCHMARK_ONE(ranged_no_components);

void build_ranged_with_components(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		state.BeginIgnoreTiming();
		ecs::add_component({0, nentities}, int{});
		ecs::commit_changes();
		state.EndIgnoreTiming();

		ecs::make([](int) {});
	}

	state.SetItemsProcessed(state.iterations());
}
ECS_BENCHMARK(ranged_with_components);


void ranged_serial_run(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::detail::_context.reset();

	ecs::detail::get_context().get_component_pool<int>();
	ecs::get_global_component<global_s>().dimension = nentities;

	ecs::make<ecs::opts::not_parallel>(benchmark);

	ecs::add_component({0, nentities}, int{});
	ecs::commit_changes();

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runs();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(ranged_serial_run);

void ranged_parallel_run(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::detail::_context.reset();

	ecs::make(benchmark);
	ecs::get_global_component<global_s>().dimension = nentities;

	ecs::add_component({0, nentities}, int{});
	ecs::commit_changes();

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runs();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(ranged_parallel_run);
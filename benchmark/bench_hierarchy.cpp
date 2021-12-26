// Benchmarks for hierarchy construction and execution

#include "gbench/include/benchmark/benchmark.h"
#include "global.h"
#include <ecs/ecs.h>
#include <random>

// A wrapper for the standard benchmark that forces a hierarchy to built
static void hierarch_lambda(ecs::entity_id id, int& i, ecs::parent<int> const& /*p*/) {
	benchmark_system(id, i);
}

static void build_hierarchies(ecs::runtime& ecs, ecs::detail::entity_type nentities) {
	// The number of children in hierarchies to test
	const int num_children = 7;

	ecs::detail::entity_type id = 0;
	ecs.add_component({0, nentities}, int{0});
	while (id < nentities) {
		ecs.add_component({id + 1, id + num_children}, ecs::parent{id + 0});
		id += 1 + num_children;
	}
	ecs.commit_changes();
}

static void build_hierarchy_with_components(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		state.BeginIgnoreTiming();
		ecs::runtime ecs;
		build_hierarchies(ecs, nentities);
		state.EndIgnoreTiming();

		ecs.make_system([](int, ecs::parent<>) {});
	}

	state.SetItemsProcessed(nentities * state.iterations());
}
//ECS_BENCHMARK(build_hierarchy_with_components);
BENCHMARK(build_hierarchy_with_components)->Arg(128 * 1024);

static void build_hierarchy_with_sub_components(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		state.BeginIgnoreTiming();
		ecs::runtime ecs;
		build_hierarchies(ecs, nentities);
		state.EndIgnoreTiming();

		ecs.make_system([](int, ecs::parent<int> const&) {});
	}

	state.SetItemsProcessed(nentities * state.iterations());
}
ECS_BENCHMARK(build_hierarchy_with_sub_components);

template <bool parallel>
void run_hierarchy(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::runtime ecs;
	auto& sys = (parallel) ? ecs.make_system(hierarch_lambda) : ecs.make_system<ecs::opts::not_parallel>(hierarch_lambda);

	build_hierarchies(ecs, nentities);

	for ([[maybe_unused]] auto const _ : state) {
		sys.run();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}

static void run_serial_hierarchy(benchmark::State& state) {
	run_hierarchy<false>(state);
}
ECS_BENCHMARK(run_serial_hierarchy);

static void run_parallel_hierarchy(benchmark::State& state) {
	run_hierarchy<true>(state);
}
ECS_BENCHMARK(run_parallel_hierarchy);

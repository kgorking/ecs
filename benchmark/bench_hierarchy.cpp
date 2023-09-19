// Benchmarks for hierarchy construction and execution

#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"
#include "global.h"

// A wrapper for the standard benchmark that forces a hierarchy to built
static void hierarch_lambda(ecs::entity_id id, int& i, ecs::parent<int> const& p) {
	benchmark_system(id, i);
	i += p.get<int>();
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

static void build_hierarchy(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::runtime ecs;
	build_hierarchies(ecs, nentities);
	auto& sys = ecs.make_system<ecs::opts::manual_update>([](int, ecs::parent<>) {});

	for ([[maybe_unused]] auto const _ : state) {
		sys.set_enable(true);
	}
}
ECS_BENCHMARK(build_hierarchy);

static void build_hierarchy_sub(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::runtime ecs;
	build_hierarchies(ecs, nentities);
	auto& sys = ecs.make_system<ecs::opts::manual_update>([](int, ecs::parent<int> const) {});

	for ([[maybe_unused]] auto const _ : state) {
		sys.set_enable(true);
	}
}
ECS_BENCHMARK(build_hierarchy_sub);

template <bool parallel>
void run_hierarchy(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	if constexpr (parallel) {
		ecs::runtime ecs;
		auto& sys = ecs.make_system<ecs::opts::manual_update>(hierarch_lambda);

		build_hierarchies(ecs, nentities);

		for ([[maybe_unused]] auto const _ : state) {
			sys.run();
		}
	} else {
		ecs::runtime ecs;
		auto& sys = ecs.make_system<ecs::opts::manual_update, ecs::opts::not_parallel>(hierarch_lambda);

		build_hierarchies(ecs, nentities);

		for ([[maybe_unused]] auto const _ : state) {
			sys.run();
		}
	}
}

static void run_hierarchy_serial(benchmark::State& state) {
	run_hierarchy<false>(state);
}
ECS_BENCHMARK(run_hierarchy_serial);

static void run_hierarchy_parallel(benchmark::State& state) {
	run_hierarchy<true>(state);
}
ECS_BENCHMARK(run_hierarchy_parallel);

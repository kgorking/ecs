#include "gbench/include/benchmark/benchmark.h"
#include <complex>
#include <ecs/ecs.h>

#include "global.h"

void build_ranged(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::runtime rt;
	rt.add_component({0, nentities}, int{});
	rt.commit_changes();

	ecs::detail::system_base& sys = rt.make_system<ecs::opts::manual_update>([](int) {});
	for ([[maybe_unused]] auto const _ : state) {
		// triggers a rebuild
		sys.set_enable(true);
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(build_ranged);

void build_many_ranged(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::runtime rt;
	for (int i = 0; i < nentities; i += 8) {
		rt.add_component({i, i + 7}, int{});
		rt.commit_changes();
	}

	ecs::detail::system_base& sys = rt.make_system<ecs::opts::manual_update>([](int) {});
	for ([[maybe_unused]] auto const _ : state) {
		// triggers a rebuild
		sys.set_enable(true);
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(build_many_ranged);

void run_serial_ranged(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::runtime rt;
	rt.make_system<ecs::opts::not_parallel>(benchmark_system);

	rt.add_component({0, nentities}, int{});
	rt.commit_changes();

	for ([[maybe_unused]] auto const _ : state) {
		rt.run_systems();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(run_serial_ranged);

void run_parallel_ranged(benchmark::State& state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::runtime rt;
	rt.make_system(benchmark_system);

	rt.add_component({0, nentities}, int{});
	rt.commit_changes();

	for ([[maybe_unused]] auto const _ : state) {
		rt.run_systems();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(run_parallel_ranged);

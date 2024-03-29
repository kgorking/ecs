#include <ecs/ecs.h>
#include <complex>

#include "gbench/include/benchmark/benchmark.h"
#include "global.h"

void build_ranged(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	ecs::runtime rt;
	rt.add_component({0, nentities}, int{});
	rt.commit_changes();

	auto& sys = rt.make_system<ecs::opts::manual_update>([](int) {});
	for ([[maybe_unused]] auto const _ : state) {
		// triggers a rebuild
		sys.set_enable(true);
	}
}
ECS_BENCHMARK(build_ranged);

void build_many_ranged(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	ecs::runtime rt;
	for (int i = 0; i < nentities; i += 8) {
		rt.add_component({i, i + 7}, int{});
		rt.commit_changes();
	}

	auto& sys = rt.make_system<ecs::opts::manual_update>([](int) {});
	for ([[maybe_unused]] auto const _ : state) {
		// triggers a rebuild
		sys.set_enable(true);
	}
}
ECS_BENCHMARK(build_many_ranged);

void run_serial_ranged(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	ecs::runtime rt;
	rt.make_system<ecs::opts::not_parallel>(benchmark_system);

	rt.add_component({0, nentities}, int{});
	rt.commit_changes();

	for ([[maybe_unused]] auto const _ : state) {
		rt.run_systems();
	}
}
ECS_BENCHMARK(run_serial_ranged);

void run_parallel_ranged(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	ecs::runtime rt;
	rt.make_system(benchmark_system);

	rt.add_component({0, nentities}, int{});
	rt.commit_changes();

	for ([[maybe_unused]] auto const _ : state) {
		rt.run_systems();
	}
}
ECS_BENCHMARK(run_parallel_ranged);

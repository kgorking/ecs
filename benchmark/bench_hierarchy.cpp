// Benchmarks for hierarchy construction and execution

#include <random>
#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"
#include "global.h"

using namespace ecs;

// A wrapper for the standard benchmark that forces a hierarchy to built
void hierarch_lambda(entity_id id, int &i, parent<int> const& /*p*/) {
	benchmark_system(id, i);
};

void build_hierarchy_no_components(benchmark::State &state) {
	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();
		ecs::make_system([](int, ecs::parent<>) {});
	}

	state.SetItemsProcessed(state.iterations());
}
ECS_BENCHMARK_ONE(build_hierarchy_no_components);

void build_hierarchy_with_components(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		state.BeginIgnoreTiming();
		ecs::add_component({0, nentities - 1}, int{});
		ecs::add_component({1, nentities}, int{}, [](ecs::entity_id id) { return ecs::parent{id - 1}; });
		ecs::commit_changes();
		state.EndIgnoreTiming();

		ecs::make_system([](int, ecs::parent<>) {});
	}

	state.SetItemsProcessed(state.iterations());
}
ECS_BENCHMARK(build_hierarchy_with_components);

void build_hierarchy_with_sub_components(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		state.BeginIgnoreTiming();
		ecs::add_component({0, nentities - 1}, int{});
		ecs::add_component({1, nentities}, int{}, [](ecs::entity_id id) { return ecs::parent{id - 1}; });
		ecs::commit_changes();
		state.EndIgnoreTiming();

		ecs::make_system([](int, ecs::parent<int> const &) {});
	}

	state.SetItemsProcessed(state.iterations());
}
ECS_BENCHMARK(build_hierarchy_with_sub_components);

template <bool parallel>
void run_hierarchy(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    detail::_context.reset();
	auto& sys = (parallel) ? make_system(hierarch_lambda) : make_system<opts::not_parallel>(hierarch_lambda);

	// The number of children in hierarchies to test
	//const int num_children = nentities/(4*std::thread::hardware_concurrency()) - 1;
	const int num_children = 7;

	detail::entity_type id = 0;
	add_component({0, nentities}, int{0});
	while (id < nentities) {
		add_component({id + 1, id + num_children}, parent{id + 0});
        id += 1 + num_children;
    }

    commit_changes();

	for ([[maybe_unused]] auto const _ : state) {
		sys.run();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}

void run_hierarchy_serial(benchmark::State& state) {
	run_hierarchy<false>(state);
}
ECS_BENCHMARK(run_hierarchy_serial);

void run_hierarchy_parallel(benchmark::State &state) {
	run_hierarchy<true>(state);
}
ECS_BENCHMARK(run_hierarchy_parallel);

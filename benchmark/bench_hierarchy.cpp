// Benchmarks for hierarchy construction and execution

#include <random>
#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"
#include "global.h"

using namespace ecs;

// A wrapper for the standard benchmark that forces a hierarcu to built
auto constexpr hierarch_lambda = [](entity_id id, int &i, parent<int> const& /*p*/, global_s const &global) {
	benchmark_system(id, i, global);
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

void run_hierarchy_serial(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    detail::_context.reset();

	ecs::get_global_component<global_s>().dimension = nentities;
	make_system<opts::not_parallel>(hierarch_lambda);

    detail::entity_type id = 0;
    while (id < nentities) {
        add_component({id + 0}, int{});
        add_component({id + 1, id + 7}, int{}, parent{id + 0});

        id += 8;
    }

    Expects(id == nentities);

    commit_changes();

    for ([[maybe_unused]] auto const _ : state) {
        run_systems();
    }

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(run_hierarchy_serial);

void run_hierarchy_parallel(benchmark::State &state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    detail::_context.reset();

	ecs::get_global_component<global_s>().dimension = nentities;
	make_system(hierarch_lambda);

    detail::entity_type id = 0;
    while (id < nentities) {
        add_component({id + 0}, int{});
        add_component({id + 1, id + 7}, int{}, parent{id + 0});

        id += 8;
    }

    Expects(id == nentities);

    commit_changes();

    for ([[maybe_unused]] auto const _ : state) {
        run_systems();
    }

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(run_hierarchy_parallel);

void run_hierarchy_parallel_rand(benchmark::State &state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    detail::_context.reset();

	ecs::get_global_component<global_s>().dimension = nentities;
	make_system(hierarch_lambda);

    std::vector<detail::entity_type> ids(nentities/8);
	std::generate(ids.begin(), ids.end(), [i = 0]() mutable { return i++ * 8; });

    std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(ids.begin(), ids.end(), g);

    for (auto id : ids) {
        add_component({id + 0}, int{-1});
        add_component({id + 1, id + 7}, int{-1}, parent{id + 0});
    }

    commit_changes();

    for ([[maybe_unused]] auto const _ : state) {
        run_systems();
    }

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(run_hierarchy_parallel_rand);

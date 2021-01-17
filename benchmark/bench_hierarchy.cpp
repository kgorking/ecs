#include <random>

#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"

#include "global.h"

using namespace ecs;

auto constexpr hierarch_lambda = [](entity_id id, int &i, parent<int> const& /*p*/, global_s const &global) {
	benchmark_system(id, i, global);
};
/*
void hierarchy_sys_build(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    for ([[maybe_unused]] auto const _ : state) {
        detail::_context.reset();

        make_system(hierarch_lambda);

        // The roots
        add_component({0, nentities}, int{});

        detail::entity_type id = 0;
        while (id < nentities) {
            // The children
            add_component({id + 1, id + 3}, parent{id + 0});

            // The grandchildren
            add_component({id + 4, id + 6}, parent{id + 1});
            add_component({id + 7, id + 9}, parent{id + 2});
            add_component({id + 10, id + 12}, parent{id + 3});

            id += 4;
        }

        commit_changes();
    }
}
ECS_BENCHMARK(hierarchy_sys_build);*/

/*
void hierarchy_serial_run(benchmark::State& state) {
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
ECS_BENCHMARK(hierarchy_serial_run);
*/
void hierarchy_parallel_run(benchmark::State& state) {
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
ECS_BENCHMARK(hierarchy_parallel_run);

void hierarchy_parallel_run_rand(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    detail::_context.reset();

	ecs::get_global_component<global_s>().dimension = nentities;
	make_system(hierarch_lambda);

    std::vector<detail::entity_type> ids(nentities/8);
	std::iota(ids.begin(), ids.end(), 0);

    std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(ids.begin(), ids.end(), g);

    for (auto id : ids) {
        add_component({id + 0}, int{});
        add_component({id + 1, id + 7}, int{}, parent{id + 0});
    }

    commit_changes();

    for ([[maybe_unused]] auto const _ : state) {
        run_systems();
    }

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(hierarchy_parallel_run_rand);

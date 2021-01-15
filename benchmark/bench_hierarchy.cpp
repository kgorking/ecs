#include <complex>
#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"

#include "global.h"

using namespace ecs;

auto constexpr hierarch_lambda = [](entity_id id, int& i, parent<int> const& p) {
    i = std::gcd((int) id, (int) p.id());
};

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
BENCHMARK(hierarchy_sys_build)->RangeMultiplier(2)->Range(32, num_components);


void hierarchy_serial_run(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    detail::_context.reset();

    make_system<opts::not_parallel>(hierarch_lambda);

    detail::entity_type id = 0;
    while (id < nentities) {
        add_component({id + 0}, int{1});

        add_component({id + 1, id + 3}, int{2}, parent{id + 0});
        add_component({id + 4, id + 6}, int{3}, parent{id + 0});
        add_component({id + 7, id + 9}, int{4}, parent{id + 0});

        id += 10;
    }

    commit_changes();

    for ([[maybe_unused]] auto const _ : state) {
        for (int i=0; i<100; i++)
            run_systems();
    }
}
BENCHMARK(hierarchy_serial_run)->RangeMultiplier(2)->Range(32, num_components);

void hierarchy_parallel_run(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    detail::_context.reset();

    make_system(hierarch_lambda);

    detail::entity_type id = 0;
    while (id < nentities) {
        add_component({id + 0}, int{1});

        add_component({id + 1, id + 3}, int{2}, parent{id + 0});
        add_component({id + 4, id + 6}, int{3}, parent{id + 0});
        add_component({id + 7, id + 9}, int{4}, parent{id + 0});

        id += 10;
    }

    commit_changes();

    for ([[maybe_unused]] auto const _ : state) {
        for (int i = 0; i < 100; i++)
            run_systems();
    }
}
BENCHMARK(hierarchy_parallel_run)->RangeMultiplier(2)->Range(32, num_components);

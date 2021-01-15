#include <complex>
#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"

#include "global.h"

using namespace ecs;

void hierarchy_sys_build(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    for ([[maybe_unused]] auto const _ : state) {
        detail::_context.reset();

        make_system([](entity_id, parent<int> const&) {});

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

#include <complex>
#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"

#include "global.h"

void children(ecs::entity_id id, ecs::parent<int> const& p) {
}

// creates 4-node cyclical trees
auto constexpr parent_generator = [](ecs::entity_id id) {
    if ((id % 4) == 0)
        return ecs::parent{id + 3};
    else
        return ecs::parent{id - 1};
};

void hierarchy_add(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    ecs::make_system(children);

    for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		ecs::add_component({0, nentities}, int{}, parent_generator);
        ecs::commit_changes();
    }
}
BENCHMARK(hierarchy_add)->RangeMultiplier(2)->Range(8, num_components);

void hierarchy_add_one_more(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    ecs::make_system(children);

    for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		ecs::add_component({0, nentities}, int{}, parent_generator);
        ecs::commit_changes();

        // will trigger complete rebuild :/
		ecs::add_component({nentities + 1}, int{}, parent_generator);
        ecs::commit_changes();
    }
}
BENCHMARK(hierarchy_add_one_more)->RangeMultiplier(2)->Range(8, num_components);

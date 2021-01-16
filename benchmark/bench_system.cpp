#include <complex>
#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"

#include "global.h"

void ranged_system_no_components(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();
		ecs::make_system([](int) {});
	}

	state.SetItemsProcessed(state.iterations());
}
ECS_BENCHMARK_ONE(ranged_system_no_components);

void ranged_system_with_components(benchmark::State &state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		state.BeginSuspendTiming();
		ecs::add_component({0, nentities}, int{});
		ecs::commit_changes();
		state.EndSuspendTiming();

		ecs::make_system([](int) {});
	}

	state.SetItemsProcessed(state.iterations());
}
ECS_BENCHMARK(ranged_system_with_components);

void hierarchy_system_no_components(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();
		ecs::make_system([](int, ecs::parent<>) {});
	}

	state.SetItemsProcessed(state.iterations());
}
ECS_BENCHMARK_ONE(hierarchy_system_no_components);

void hierarchy_system_with_components(benchmark::State &state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		state.BeginSuspendTiming();
		ecs::add_component({0, nentities-1}, int{});
		ecs::add_component({1, nentities}, int{}, [](ecs::entity_id id) { return ecs::parent{id - 1}; });
		ecs::commit_changes();
		state.EndSuspendTiming();

		ecs::make_system([](int, ecs::parent<>) {});
	}

	state.SetItemsProcessed(state.iterations());
}
ECS_BENCHMARK(hierarchy_system_with_components);

void hierarchy_system_with_sub_components(benchmark::State &state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		state.BeginSuspendTiming();
		ecs::add_component({0, nentities-1}, int{});
		ecs::add_component({1, nentities}, int{}, [](ecs::entity_id id) { return ecs::parent{id - 1}; });
		ecs::commit_changes();
		state.EndSuspendTiming();

		ecs::make_system([](int, ecs::parent<int> const&) {});
	}

	state.SetItemsProcessed(state.iterations());
}
ECS_BENCHMARK(hierarchy_system_with_sub_components);

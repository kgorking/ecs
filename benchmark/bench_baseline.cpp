#include "gbench/include/benchmark/benchmark.h"
#include <ecs/ecs.h>

#include "global.h"

void raw_serial_run(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::detail::_context.reset();

	auto &global = ecs::get_global_component<global_s>();
	global.dimension = nentities;

	std::vector<int> colors(nentities + 1);
	std::fill_n(colors.begin(), nentities, int{});

	ecs::entity_range const range{0, nentities};

	for ([[maybe_unused]] auto const _ : state) {
		std::for_each(std::execution::seq, range.begin(), range.end(),
					  [&](ecs::entity_id ent) { benchmark_system(ent, colors[ent], global); });
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(raw_serial_run);

void raw_parallel_run(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::detail::_context.reset();

	auto &global = ecs::get_global_component<global_s>();
	global.dimension = nentities;

	std::vector<int> colors(nentities + 1);
	std::fill_n(colors.begin(), nentities, int{});

	ecs::entity_range const range{0, nentities};

	for ([[maybe_unused]] auto const _ : state) {
		std::for_each(std::execution::par, range.begin(), range.end(),
					  [&](ecs::entity_id ent) { benchmark_system(ent, colors[ent], global); });
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(raw_parallel_run);

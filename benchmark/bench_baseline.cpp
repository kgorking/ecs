#include "gbench/include/benchmark/benchmark.h"
#include <algorithm>
#include <ecs/ecs.h>
#include <future>

#include "global.h"

template <bool parallel>
void run_raw(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	std::vector<int> colors(nentities + 1);
	std::fill_n(colors.begin(), nentities, int{});

	ecs::entity_range const range{0, nentities};

	auto constexpr e_p = std::conditional_t<parallel, std::execution::parallel_policy, std::execution::sequenced_policy>{};
	for ([[maybe_unused]] auto const _ : state) {
		std::for_each(e_p, range.begin(), range.end(), [&](ecs::entity_id ent) { benchmark_system(ent, colors[ent]); });
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}

// Simulates the hierarchial setup from system_hiearchy
template <bool parallel>
void run_raw_hierarchy(benchmark::State &state) {
	auto const nentities = static_cast<size_t>(state.range(0));

	//
	// Simulate the central storage.

	std::vector<int> colors(nentities + 1);
	std::fill_n(colors.begin(), nentities + 1, 0);

	//
	// Simulate a work unit that operates on 'colors'.

	using argument = std::tuple<ecs::entity_id, int *>;

	// Create the 'colors' arguments.
	// Holds an id and a pointer to that id's color.
	std::vector<argument> args;
	std::generate_n(std::back_inserter(args), nentities + 1, [i = 0, &colors]() mutable {
		auto a = argument(ecs::entity_id{i}, colors.data() + i);
		i++;
		return a;
	});

	// A span which covers all the arguments.
	auto const span_all_args = std::span(args);

	// Create spans over clusters of arguments.
	// Arguments in clusters must be processed serially.
	const size_t span_size = 8;
	std::vector<std::span<argument>> argument_spans;
	size_t count = 0;
	while (count + span_size < nentities) {
		auto const cluster = span_all_args.subspan(count+1, span_size-1);
		argument_spans.push_back(cluster);
		count += span_size;
	}
	argument_spans.push_back(span_all_args.subspan(count, nentities - count));

	// Determine the execution policy to use
	auto constexpr e_p = std::conditional_t<parallel, std::execution::parallel_policy, std::execution::sequenced_policy>{};

	for ([[maybe_unused]] auto const _ : state) {
		std::for_each(e_p, argument_spans.begin(), argument_spans.end(), [](std::span<argument> const local_span) {
			for (argument &arg : local_span) {
				auto const ent = std::get<ecs::entity_id>(arg);
				int &color = *std::get<int *>(arg);
				benchmark_system(ent, color);
			}
		});
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}

void run_raw_serial(benchmark::State &state) {
	run_raw<false>(state);
}
ECS_BENCHMARK(run_raw_serial);

void run_raw_parallel(benchmark::State &state) {
	run_raw<true>(state);
}
ECS_BENCHMARK(run_raw_parallel);

void run_raw_hierarchy_serial(benchmark::State &state) {
	run_raw_hierarchy<false>(state);
}
ECS_BENCHMARK(run_raw_hierarchy_serial);

void run_raw_hierarchy_parallel(benchmark::State &state) {
	run_raw_hierarchy<true>(state);
}
ECS_BENCHMARK(run_raw_hierarchy_parallel);

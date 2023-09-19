#include "gbench/include/benchmark/benchmark.h"

#include "global.h"
#include <execution>
#include <iterator>
#include <span>
#include <tuple>
#include <type_traits>
#include <vector>
#include <ecs/entity_id.h>
#include <ecs/entity_range.h>

template <bool parallel>
void run_raw(benchmark::State &state) {
	auto const num_items = state.range(0);
	auto const num_entities = static_cast<ecs::detail::entity_type>(num_items);
	auto const num_colors = static_cast<size_t>(num_items) + 1;

	std::vector<int> colors(num_colors);
	std::fill_n(colors.begin(), num_colors, int{});

	ecs::entity_range const range{0, num_entities};

	auto constexpr e_p = std::conditional_t<parallel, std::execution::parallel_policy, std::execution::sequenced_policy>{};
	for ([[maybe_unused]] auto const _ : state) {
		std::for_each(e_p, range.begin(), range.end(), [&](ecs::entity_id ent) {
			auto const index = static_cast<size_t>(ent);
			benchmark_system(ent, colors[index]);
		});
	}
}

// Simulates the hierarchial setup from system_hiearchy
template <bool parallel>
void run_hierarchy_raw(benchmark::State &state) {
	auto const num_items = static_cast<std::size_t>(state.range(0));
	//auto const num_entities = static_cast<ecs::detail::entity_type>(num_items);
	auto const num_colors = static_cast<size_t>(num_items) + 1;


	//
	// Simulate the central storage.

	std::vector<int> colors(num_colors);
	std::fill_n(colors.begin(), num_colors, 0);

	//
	// Simulate a work unit that operates on 'colors'.

	using argument = std::tuple<ecs::entity_id, int *>;

	// Create the 'colors' arguments.
	// Holds an id and a pointer to that id's color.
	std::vector<argument> args;
	std::generate_n(std::back_inserter(args), num_colors, [i = 0, &colors]() mutable {
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
	while (count + span_size < num_colors) {
		auto const cluster = span_all_args.subspan(count+1, span_size-1);
		argument_spans.push_back(cluster);
		count += span_size;
	}
	argument_spans.push_back(span_all_args.subspan(count, num_items - count));

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
}

void run_serial_raw(benchmark::State &state) {
	run_raw<false>(state);
}
ECS_BENCHMARK(run_serial_raw);

void run_parallel_raw(benchmark::State &state) {
	run_raw<true>(state);
}
ECS_BENCHMARK(run_parallel_raw);

void run_serial_hierarchy_raw(benchmark::State& state) {
	run_hierarchy_raw<false>(state);
}
ECS_BENCHMARK(run_serial_hierarchy_raw);

void run_parallel_hierarchy_raw(benchmark::State &state) {
	run_hierarchy_raw<true>(state);
}
ECS_BENCHMARK(run_parallel_hierarchy_raw);

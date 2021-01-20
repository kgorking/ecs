#include "gbench/include/benchmark/benchmark.h"
#include <ecs/ecs.h>

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

void volatile_shenanigans(char const volatile *) {}

template <class T>
void do_not_opt(T const &t) {
	volatile_shenanigans(&reinterpret_cast<char const volatile &>(t));
}

#include <complex>
int custom_system(ecs::entity_id ent) {
	constexpr int dimension = 16384;
	constexpr int max_iterations = 500;
	constexpr double fr_w = 1.5;
	constexpr double fr_h = 1.5;
	constexpr double fr_x = -2.2;
	constexpr double fr_y = 1.2;

	size_t const x = ent % dimension;
	size_t const y = ent / dimension;

	std::complex<double> c(static_cast<double>(x), static_cast<double>(y));

	// Convert a pixel coordinate to the complex domain
	c = {c.real() / (double)dimension * fr_w + fr_x, c.imag() / (double)dimension * fr_h + fr_y};

	// Check if a point is in the set or escapes to infinity
	std::complex<double> z(0);
	int iter = 0;
	while (abs(z) < 3 && iter < max_iterations) {
		z = z * z + c;
		iter++;
	}

	return iter;
}

// Simulates the hierarchial setup from system_hiearchy
template <bool parallel>
void run_raw_hierarchy(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	//
	// Simulate the central storage.
	
	std::vector<int> colors(nentities + 1);
	std::fill_n(colors.begin(), nentities + 1, 0);

	//
	// Simulate a work unit that operates on 'colors'.

	// Create the 'colors' arguments,
	// which holds an id and a pointer to that id's color.
	using argument = std::tuple<ecs::entity_id, int *>;
	std::vector<argument> args;
	std::generate_n(std::back_inserter(args), nentities + 1, [i = 0, &colors]() { return argument(ecs::entity_id{i}, colors.data() + i); });

	// A span which covers all the arguments.
	auto const span_all_args = std::span(args);

	// Create spans over clusters of arguments.
	// Arguments in clusters must be processed serially.
	constexpr size_t span_size = 4;
	std::vector<std::span<argument>> argument_spans;
	size_t count = 0;
	while (count < nentities) {
		// 'count' is the root, and is not processed
		argument_spans.push_back(span_all_args.subspan(count, span_size));
		count += span_size;
	}

	// Determine the execution policy to use
	auto constexpr e_p = std::conditional_t<parallel, std::execution::parallel_policy, std::execution::sequenced_policy>{};

	for ([[maybe_unused]] auto const _ : state) {
		std::for_each(e_p, argument_spans.begin(), argument_spans.end(), [](auto const local_span) {
			for (argument& arg : local_span) {
				auto const ent = std::get<ecs::entity_id>(arg);

				int color = *std::get<int *>(arg);
				benchmark_system(ent, color);

				*std::get<int *>(arg) = color;			// Writing back the value results in serial execution time.
				//benchmark::DoNotOptimize(color);		// Not writing to it parallelizes just fine
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

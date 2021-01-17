#include "gbench/include/benchmark/benchmark.h"
#include <complex>
#include <ecs/ecs.h>

#include "global.h"

void benchmark_system(ecs::entity_id ent, int &color, global_s const &global) {
	constexpr int max_iterations = 500;
	constexpr double fr_w = 1.5;
	constexpr double fr_h = 1.5;
	constexpr double fr_x = -2.2;
	constexpr double fr_y = 1.2;

	size_t const x = ent % global.dimension;
	size_t const y = ent / global.dimension;

	std::complex<double> c(static_cast<double>(x), static_cast<double>(y));

	// Convert a pixel coordinate to the complex domain
	c = {c.real() / (double)global.dimension * fr_w + fr_x, c.imag() / (double)global.dimension * fr_h + fr_y};

	// Check if a point is in the set or escapes to infinity
	std::complex<double> z(0);
	int iter = 0;
	while (abs(z) < 3 && iter < max_iterations) {
		z = z * z + c;
		iter++;
	}

	color = iter;
};

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
		//for (ecs::entity_id ent{0}; ent <= nentities; ent++) {
		std::for_each(std::execution::par, range.begin(), range.end(),
			[&](ecs::entity_id ent) { benchmark_system(ent, colors[ent], global); });
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(raw_parallel_run);

void ranged_serial_run(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::detail::_context.reset();

	ecs::detail::get_context().get_component_pool<int>();
	ecs::get_global_component<global_s>().dimension = nentities;

	ecs::make_system<ecs::opts::not_parallel>(benchmark_system);

	ecs::add_component({0, nentities}, int{});
	ecs::commit_changes();

	for ([[maybe_unused]] auto const _ : state) {
		ecs::run_systems();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(ranged_serial_run);

void ranged_parallel_run(benchmark::State &state) {
	auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::detail::_context.reset();

	ecs::make_system(benchmark_system);
	ecs::get_global_component<global_s>().dimension = nentities;

	ecs::add_component({0, nentities}, int{});
	ecs::commit_changes();

	for ([[maybe_unused]] auto const _ : state) {
		ecs::run_systems();
	}

	state.SetItemsProcessed(state.iterations() * nentities);
}
ECS_BENCHMARK(ranged_parallel_run);
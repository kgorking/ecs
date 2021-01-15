#include <complex>
#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"

#include "global.h"

auto constexpr benchmark_system = [](ecs::entity_id ent, int& color, global_s const& global) {
	constexpr int max_iterations = 50;
	constexpr double fr_w = 1.5;
	constexpr double fr_h = 1.5;
	constexpr double fr_x = -2.2;
	constexpr double fr_y = 1.2;

	size_t const x = ent % global.dimension;
    size_t const y = ent / global.dimension;

	std::complex<double> c(static_cast<double>(x), static_cast<double>(y));

	// Convert a pixel coordinate to the complex domain
    c = {c.real() / (double) global.dimension * fr_w + fr_x, c.imag() / (double) global.dimension * fr_h + fr_y};

	// Check if a point is in the set or escapes to infinity
	std::complex<double> z(0);
	int iter = 0;
	while (abs(z) < 3 && iter < max_iterations) {
		z = z * z + c;
		iter++;
	}

	color = iter;
};

void raw_update(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	auto & global = ecs::get_global_component<global_s>();
    global.dimension = nentities;

	std::vector<int> colors(nentities+1);
	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		std::fill_n(colors.begin(), nentities, int{});
		for (ecs::entity_id ent{ 0 }; ent <= nentities; ent++) {
            benchmark_system(ent, colors[ent], global);
		}
	}
}
BENCHMARK(raw_update)->Arg(num_components);

void system_update(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	ecs::detail::get_context().get_component_pool<int>();
    ecs::get_global_component<global_s>().dimension = nentities;
	
	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();
        ecs::make_system<ecs::opts::not_parallel>(benchmark_system);

		ecs::add_component({0, nentities}, int{});
		ecs::update();
	}
}
BENCHMARK(system_update)->Arg(num_components);

void system_update_parallel(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		ecs::make_system(benchmark_system);
		ecs::get_global_component<global_s>().dimension = nentities;

		ecs::add_component({ 0, nentities }, int{});
		ecs::update();
	}
}
BENCHMARK(system_update_parallel)->Arg(num_components);

void system_register(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

        ecs::add_component({0, nentities}, int{});
		ecs::commit_changes();
		ecs::make_system(benchmark_system);
	}
}
BENCHMARK(system_register)->Arg(num_components);

void system_register_and_unregister(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		ecs::add_component({0, nentities}, int{});
		ecs::commit_changes();
		ecs::make_system(benchmark_system);

		ecs::remove_component<int>({0, nentities});
		ecs::commit_changes();
	}
}
BENCHMARK(system_register_and_unregister)->Arg(num_components);

void system_register_and_unregister_half_middle(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

        ecs::add_component({0, nentities}, int{});
		ecs::commit_changes();
		ecs::make_system(benchmark_system);

		ecs::remove_component<int>({nentities/4, nentities - nentities/4});
		ecs::commit_changes();
	}
}
BENCHMARK(system_register_and_unregister_half_middle)->Arg(num_components);

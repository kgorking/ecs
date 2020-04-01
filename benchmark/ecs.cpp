#include <string>
#include <random>
#include <complex>
#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"

//size_t constexpr num_components = 32;
//size_t constexpr num_components = 4 * 1024;
//size_t constexpr num_components = 32 * 1024;
size_t constexpr num_components = 256 * 1024;
//size_t constexpr num_components = 1024 * 1024;
//size_t constexpr num_components = 16 * 1024 * 1024;
//size_t constexpr num_components = 1024 * 1024 * 1024;  // 1 billion entities

// 514868000 ns
// 459405950 ns
// 463761700 ns
// 602081200 ns
// 522946700 ns
// 498973400 ns

struct shared_s {
	ecs_flags(ecs::share);
	size_t dimension;
};

auto constexpr benchmark_system = [](ecs::entity_id ent, int& color, shared_s const& shared) {
	constexpr int max_iterations = 50;
	constexpr double fr_w = 1.5;
	constexpr double fr_h = 1.5;
	constexpr double fr_x = -2.2;
	constexpr double fr_y = 1.2;

	size_t const x = ent % shared.dimension;
	size_t const y = ent / shared.dimension;

	std::complex<double> c(static_cast<double>(x), static_cast<double>(y));

	// Convert a pixel coordinate to the complex domain
	c = { c.real() / (double)shared.dimension * fr_w + fr_x, c.imag() / (double)shared.dimension * fr_h + fr_y };

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
	auto const nentities = static_cast<ecs::entity_type>(state.range(0));

	std::vector<int> colors(nentities);
	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		auto & shared = ecs::get_shared_component<shared_s>();
		shared.dimension = nentities;

		std::fill_n(colors.begin(), nentities, int{});
		for (ecs::entity_id ent{ 0 }; ent < nentities; ent++) {
			benchmark_system(ent, colors[ent], shared);
		}
	}
}
BENCHMARK(raw_update)->Range(num_components, num_components);

void system_update(benchmark::State& state) {
	auto const nentities = static_cast<ecs::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		ecs::make_system(benchmark_system);
		ecs::get_shared_component<shared_s>().dimension = nentities;

		ecs::add_components({0, nentities}, int{}, shared_s{});
		ecs::update_systems();
	}
}
BENCHMARK(system_update)->Range(num_components, num_components);

void system_update_parallel(benchmark::State& state) {
	auto const nentities = static_cast<ecs::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::detail::_context.reset();

		ecs::make_parallel_system(benchmark_system);
		ecs::get_shared_component<shared_s>().dimension = nentities;

		ecs::add_components({ 0, nentities }, int{}, shared_s{});
		ecs::update_systems();
	}
}
BENCHMARK(system_update_parallel)->Range(num_components, num_components);

void component_add(benchmark::State& state) {
	auto const nentities = static_cast<ecs::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		state.PauseTiming();
			ecs::detail::_context.reset();
			ecs::make_system([](ecs::entity /*ent*/, size_t const& /*unused*/) { });
		state.ResumeTiming();

		ecs::add_component({ 0, nentities }, size_t{});
		ecs::commit_changes();
	}
}
BENCHMARK(component_add)->Range(num_components, num_components);

void component_randomized_add(benchmark::State& state) {
	auto const nentities = static_cast<ecs::entity_type>(state.range(0));

	std::vector<ecs::entity_id> ids;
	ids.reserve(nentities);
	std::generate_n(std::back_inserter(ids), nentities, [i = std::int32_t{ 0 }]() mutable { return i++; });
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(ids.begin(), ids.end(), g);

	for ([[maybe_unused]] auto const _ : state) {
		state.PauseTiming();
			ecs::detail::_context.reset();
			ecs::make_system(benchmark_system);
			ecs::get_shared_component<shared_s>().dimension = nentities;
		state.ResumeTiming();

		for (auto id : ids) {
			ecs::add_component(id, int{});
		}
		ecs::commit_changes();
	}
}
BENCHMARK(component_randomized_add)->Range(num_components, num_components);

void component_remove(benchmark::State& state) {
	auto const nentities = static_cast<ecs::entity_type>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		state.PauseTiming();
			ecs::detail::_context.reset();
			ecs::make_system(benchmark_system);
			ecs::get_shared_component<shared_s>().dimension = nentities;
		state.ResumeTiming();

		ecs::add_component({ 0, nentities }, int{});
		ecs::commit_changes();

		ecs::remove_component<int>({ 0, nentities });
		ecs::commit_changes();
	}
}
BENCHMARK(component_remove)->Range(num_components, num_components);

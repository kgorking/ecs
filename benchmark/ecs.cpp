#include <string>
#include <random>
#include <complex>
#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"

//size_t constexpr start_range = 4096;
//size_t constexpr end_range = 1024 * 1024;
size_t constexpr start_range = 32;
//size_t constexpr end_range = 256 * 1024;
//size_t constexpr start_range = 256 * 1024;
size_t constexpr end_range = 16 * 1024 * 1024;
//size_t constexpr end_range = 1024 * 1024 * 1024;  // 1 billion entities

struct shared_s : ecs::shared {
	size_t dimension;
};

auto constexpr benchmark_system = [](ecs::entity_id ent, int& color, shared_s const& shared) {
	constexpr size_t max_iterations = 500;
	constexpr double fr_w = 1.5;
	constexpr double fr_h = 1.5;
	constexpr double fr_x = -2.2;
	constexpr double fr_y = 1.2;

	size_t const x = ent.id % shared.dimension;
	size_t const y = ent.id / shared.dimension;

	std::complex<double> c(static_cast<double>(x), static_cast<double>(y));

	// Convert a pixel coordinate to the complex domain
	c = { c.real() / (double)shared.dimension * fr_w + fr_x, c.imag() / (double)shared.dimension * fr_h + fr_y };

	// Check if a point is in the set or escapes to infinity
	std::complex<double> z(0);
	int iter = 0;
	while (abs(z) < 3.0 && iter < max_iterations) {
		z = z * z + c;
		iter++;
	}

	color = iter;
};

void raw_update(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	struct vec_s : std::vector<int>, ecs::shared {};
	for (auto const _ : state) {
		ecs::runtime::reset();
		shared_s& shared = *ecs::get_shared_component<shared_s>();
		shared.dimension = nentities;
		vec_s* colors = ecs::get_shared_component<vec_s>();
		colors->resize(nentities);

		for (auto i = 0u; i < nentities; i++)
			benchmark_system(i, colors->data()[i], shared);
	}
}
BENCHMARK(raw_update)->RangeMultiplier(2)->Range(start_range, end_range);

void system_update(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	for (auto const _ : state) {
		ecs::runtime::reset();
		ecs::add_system(benchmark_system);
		ecs::get_shared_component<shared_s>()->dimension = nentities;

		ecs::add_component_range(0, nentities, shared_s{});
		ecs::add_component_range(0, nentities, int{});
		ecs::commit_changes();
		ecs::run_systems();
	}
}
BENCHMARK(system_update)->RangeMultiplier(2)->Range(start_range, end_range);

void system_update_parallel(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	for (auto const _ : state) {
		ecs::runtime::reset();
		ecs::add_system_parallel(benchmark_system);
		ecs::get_shared_component<shared_s>()->dimension = nentities;

		ecs::add_component_range(0, nentities, shared_s{});
		ecs::add_component_range(0, nentities, int{});
		ecs::commit_changes();
		ecs::run_systems();
	}
}
BENCHMARK(system_update_parallel)->RangeMultiplier(2)->Range(start_range, end_range);

void component_add(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	for (auto const _ : state) {
		state.PauseTiming();
		ecs::runtime::reset();
		ecs::runtime::init_components<float>();
		ecs::add_system([](ecs::entity ent, size_t const&) {
			ent.add(3.14f);
		});
		state.ResumeTiming();

		ecs::add_component_range(0, nentities, size_t{});
		ecs::commit_changes();

		ecs::run_systems();		// adds the floats
		ecs::commit_changes();	// commits them to the runtime
	}
}
BENCHMARK(component_add)->RangeMultiplier(2)->Range(start_range, end_range);

void component_add_parallel(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	for (auto const _ : state) {
		state.PauseTiming();
		ecs::runtime::reset();
		ecs::runtime::init_components<float>();
		ecs::add_system_parallel([](ecs::entity ent, size_t const&) {
			ent.add(3.14f);
		});
		state.ResumeTiming();

		ecs::add_component_range(0, nentities, size_t{});
		ecs::commit_changes();

		ecs::run_systems();		// adds the floats
		ecs::commit_changes();	// commits them to the runtime
	}
}
BENCHMARK(component_add_parallel)->RangeMultiplier(2)->Range(start_range, end_range);

void component_randomized_add(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	for (auto const _ : state) {
		state.PauseTiming();
			ecs::runtime::reset();
			ecs::add_system(benchmark_system);
			ecs::get_shared_component<shared_s>()->dimension = nentities;

			std::vector<ecs::entity_id> ids(nentities);
			std::iota(ids.begin(), ids.end(), 0);

			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(ids.begin(), ids.end(), g);
		state.ResumeTiming();

		for (auto id : ids) {
			ecs::add_component(id, int{});
		}
		ecs::commit_changes();
	}
}
BENCHMARK(component_randomized_add)->RangeMultiplier(2)->Range(start_range, end_range);

void component_remove(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	for (auto const _ : state) {
		state.PauseTiming();
			ecs::runtime::reset();
			ecs::add_system(benchmark_system);
			ecs::get_shared_component<shared_s>()->dimension = nentities;
		state.ResumeTiming();

		ecs::add_component_range(0, nentities, int{});
		ecs::commit_changes();

		ecs::remove_component_range<int>(0, nentities);
		ecs::commit_changes();
	}
}
BENCHMARK(component_remove)->RangeMultiplier(2)->Range(start_range, end_range);

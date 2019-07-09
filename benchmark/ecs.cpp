#include <string>
#include <random>
#include <complex>
#include <ecs/ecs.h>
#include "benchmark/benchmark.h"

//size_t constexpr start_range = 4096;
//size_t constexpr end_range = 1024 * 1024;
//size_t constexpr start_range = 32;
//size_t constexpr end_range = 256 * 1024;
size_t constexpr start_range = 256 * 1024;
size_t constexpr end_range = 20 * 1024 * 1024;

struct mandelbrot_shared : ecs::shared {
	size_t dimension;
};

auto constexpr mandelbrot_system = [](ecs::entity_id ent, size_t &color, mandelbrot_shared const& shared) {
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
	size_t iter = 0;
	while (abs(z) < 3.0 && iter < max_iterations) {
		z = z * z + c;
		iter++;
	}

	color += iter;
};

void raw_update(benchmark::State& state) {
	int64_t const nentities = state.range(0);
	mandelbrot_shared shared;
	shared.dimension = nentities;

	for (auto const _ : state) {
		state.PauseTiming();
		state.ResumeTiming();

		auto colors = std::vector<size_t>(nentities);
		for (auto i = 0u; i < nentities; i++) {
			mandelbrot_system(i, colors.at(i), shared);
		}
	}

	state.SetItemsProcessed(nentities * state.iterations());
	state.SetBytesProcessed(nentities * state.iterations() * (sizeof(size_t)));
}
BENCHMARK(raw_update)->RangeMultiplier(2)->Range(start_range, end_range);

void system_update(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	for (auto const _ : state) {
		state.PauseTiming();
			ecs::runtime::reset();
			ecs::add_system(mandelbrot_system);
			ecs::get_shared_component<mandelbrot_shared>()->dimension = nentities;
			ecs::add_component_range<mandelbrot_shared>(0, nentities);
		state.ResumeTiming();

		ecs::add_component_range<size_t>(0, nentities);
		ecs::update_systems();
	}

	state.SetItemsProcessed(nentities * state.iterations());
	state.SetBytesProcessed(nentities * state.iterations() * sizeof(size_t) + sizeof(mandelbrot_shared));
}
BENCHMARK(system_update)->RangeMultiplier(2)->Range(start_range, end_range);

void component_add(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	for (auto const _ : state) {
		state.PauseTiming();
		ecs::runtime::reset();
		ecs::runtime::init_components<float>();
		ecs::add_system([](ecs::entity ent, size_t const&) {
			ent.add(3.14f);
		});

		ecs::add_component_range<size_t>(0, nentities);
		ecs::commit_changes();
		state.ResumeTiming();

		ecs::run_systems();		// adds the floats
	
		state.PauseTiming();
		ecs::commit_changes();	// commits them to the runtime
		state.ResumeTiming();
	}

	state.SetItemsProcessed(nentities * state.iterations());
	state.SetBytesProcessed(nentities * state.iterations() * (sizeof(size_t)));
}
BENCHMARK(component_add)->RangeMultiplier(2)->Range(start_range, end_range);

void component_add_concurrent(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	for (auto const _ : state) {
		state.PauseTiming();
		ecs::runtime::reset();
		ecs::runtime::init_components<float>();
		ecs::add_system_parallel([](ecs::entity ent, size_t const&) {
			ent.add(3.14f);
		});

		ecs::add_component_range<size_t>(0, nentities);
		ecs::commit_changes();
		state.ResumeTiming();

		ecs::run_systems();		// adds the floats

		state.PauseTiming();
		ecs::commit_changes();	// commits them to the runtime
		state.ResumeTiming();
	}

	state.SetItemsProcessed(nentities * state.iterations());
	state.SetBytesProcessed(nentities * state.iterations() * (sizeof(size_t)));
}
BENCHMARK(component_add_concurrent)->RangeMultiplier(2)->Range(start_range, end_range);

void component_randomized_add(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	for (auto const _ : state) {
		state.PauseTiming();
			ecs::runtime::reset();
			ecs::add_system(mandelbrot_system);

			std::vector<ecs::entity_id> ids(nentities);
			std::iota(ids.begin(), ids.end(), 0);

			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(ids.begin(), ids.end(), g);
		state.ResumeTiming();

		for (auto id : ids) {
			ecs::add_component<size_t>(id);
		}
		ecs::commit_changes();
	}

	state.SetItemsProcessed(nentities * state.iterations());
	state.SetBytesProcessed(nentities * state.iterations() * (sizeof(size_t)));
}
BENCHMARK(component_randomized_add)->RangeMultiplier(2)->Range(start_range, end_range);

void component_remove(benchmark::State& state) {
	uint32_t const nentities = gsl::narrow_cast<uint32_t>(state.range(0));

	for (auto const _ : state) {
		state.PauseTiming();
			ecs::runtime::reset();
			ecs::add_system(mandelbrot_system);

			ecs::add_component_range<size_t>(0, nentities);
			ecs::commit_changes();
		state.ResumeTiming();

		ecs::remove_component_range<size_t>(0, nentities);
		ecs::commit_changes();
	}

	state.SetItemsProcessed(nentities * state.iterations());
	state.SetBytesProcessed(nentities * state.iterations() * (sizeof(size_t)));
}
BENCHMARK(component_remove)->RangeMultiplier(2)->Range(start_range, end_range);

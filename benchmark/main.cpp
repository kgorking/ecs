#include "benchmark/benchmark.h"


#include "global.h"
#include <ecs/entity_id.h>
#include <complex>

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
}

BENCHMARK_MAIN();

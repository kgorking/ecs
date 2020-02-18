#include <complex>
#include <iostream>
#include <ecs/ecs.h>
#include <ecs/entity_range.h>

// Based on https://solarianprogrammer.com/2013/02/28/mandelbrot-set-cpp-11/

constexpr size_t dimension = 500;

auto constexpr mandelbrot_system = [](ecs::entity_id ent, size_t &color) noexcept {
	size_t constexpr max_iterations = 500;
	double constexpr fr_w = 1.5;
	double constexpr fr_h = 1.5;
	double constexpr fr_x = -2.2;
	double constexpr fr_y = 1.2;

	size_t x = ent.id % dimension;
	size_t y = ent.id / dimension;

	std::complex<double> c(static_cast<double>(x), static_cast<double>(y));

	// Convert a pixel coordinate to the complex domain
	c = { c.real() / (double)dimension * fr_w + fr_x, c.imag() / (double)dimension * fr_h + fr_y };

	// Check if a point is in the set or escapes to infinity
	std::complex<double> z(0);
	size_t iter = 0;
	while (abs(z) < 3.0 && iter < max_iterations) {
		z = z * z + c;
		iter++;
	}
	
	color += iter;
};

int main()
{
	// Add the system
	ecs::add_system(mandelbrot_system);

	// Add the size_t component to the pixels/entities
	ecs::entity_range ents{ 0, dimension*dimension, size_t{ 0 } };

	// Commit all component changes and run the system
	ecs::update_systems();

	// Count the pixels equal to one
	size_t counter = 0;
	for (size_t const& color : ents.get<size_t>())
		if (color == 1)
			counter++;

	std::cout << counter << " pixels with a value of 1\n";
}

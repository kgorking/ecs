#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Test two systems with two components")
{
	ecs::runtime::reset();

	int a = 0, b = 0;

	// Add some systems to test
	ecs::add_system([&a, &b](int const&, unsigned const&) { a++; b++; });
	ecs::add_system([&a, &b](int const&, float const&) { a++; b++; });

	// Create 100 entities and add stuff to them
	ecs::add_component_range(0, 99, int{ 1 });
	ecs::add_component_range(0, 99, unsigned{ 2 });
	ecs::add_component_range(4, 22, float{ 3 });
	ecs::commit_changes();

	// Run the system
	ecs::run_systems();

	// Check stuff
	CHECK(a == b);
	//CHECK(a == 100 * 2);
	//CHECK(b == 100 * 2);
}

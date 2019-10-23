#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Two systems with two components", "[system][component]")
{
	ecs::context::reset();

	int a = 0, b = 0;

	// Add some systems to test
	ecs::add_system([&a, &b](int const&, unsigned const&) { a++; b++; });
	ecs::add_system([&a, &b](int const&, float const&) { a++; b++; });

	// Create 100 entities and add stuff to them
	ecs::add_component({ 0, 9 }, int{ 1 });
	ecs::add_component({ 0, 9 }, unsigned{ 2 });
	ecs::add_component({ 4, 7 }, float{ 3 });
	ecs::commit_changes();

	// Run the system
	ecs::run_systems();

	// Check stuff
	CHECK(a == b);
	//CHECK(a == 100 * 2);
	//CHECK(b == 100 * 2);
}

#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Two systems with two components", "[system][component]")
{
	ecs::detail::_context.reset();

	int a = 0;
	int b = 0;

	// Add some systems to test
	ecs::make_system([&a, &b](int const& /*i*/, unsigned const& /*u*/) { a++; b++; });
	ecs::make_system([&a, &b](int const& /*i*/, float const& /*f*/) { a++; b++; });

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

#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("System with two components", "[system][component]")
{
	ecs::detail::_context.reset();

	int a = 0, b = 0;

	// Add some systems to test
	ecs::add_system([&a](int const&) { a++; });
	ecs::add_system([&b](unsigned const&) { b++; });
	ecs::add_system([&a, &b](int const&, unsigned const&) { a++; b++; });

	// Add components to 10 entities
	ecs::add_component({ 0, 9 }, int{ 1 });
	ecs::add_component({ 0, 9 }, unsigned{ 2 });
	ecs::commit_changes();

	// Run the system
	ecs::run_systems();

	// Check stuff
	CHECK(a == b);
	CHECK(a == 10 * 2);
}

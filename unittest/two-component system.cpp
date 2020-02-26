#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("System with two components", "[system][component]")
{
	ecs::detail::_context.reset();

	struct local1 { int i; };
	struct local2 { int i; };

	int a = 0;
	int b = 0;

	// Add some systems to test
	ecs::make_system([&a](local1 const& /*i*/) { a++; });
	ecs::make_system([&b](local2 const& /*u*/) { b++; });
	ecs::make_system([&a, &b](local1 const& /*i*/, local2 const& /*u*/) { a++; b++; });

	// Add components to 10 entities
	ecs::add_component({ 0, 9 }, local1{ 1 });
	ecs::add_component({ 0, 9 }, local2{ 2 });
	ecs::commit_changes();

	// Run the system
	ecs::run_systems();

	// Check stuff
	CHECK(a == b);
	CHECK(a == 10 * 2);
}

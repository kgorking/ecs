#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Two systems with two components", "[system][component]")
{
	ecs::detail::_context.reset();

	struct local1 { int i; };
	struct local2 { int i; };
	struct local3 { int i; };

	int a = 0;
	int b = 0;

	// Add some systems to test
	ecs::make_system([&a, &b](local1 const& /*i*/, local2 const& /*u*/) { a++; b++; });
	ecs::make_system([&a, &b](local1 const& /*i*/, local3 const& /*f*/) { a++; b++; });

	// Create 100 entities and add stuff to them
	ecs::add_component({ 0, 9 }, local1{ 0 });
	ecs::add_component({ 0, 9 }, local2{ 0 });
	ecs::add_component({ 4, 7 }, local3{ 0 });
	ecs::commit_changes();

	// Run the system
	ecs::run_systems();

	// Check stuff
	CHECK(a == b);
	//CHECK(a == 100 * 2);
	//CHECK(b == 100 * 2);
}

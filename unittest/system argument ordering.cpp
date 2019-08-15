#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Components are passed in the correct order to the system", "[system][ordering]")
{
	ecs::runtime::reset();

	struct C_Order1 { unsigned i; };
	struct C_Order2 { unsigned j; };

	// Add a system to check the order
	ecs::add_system([](C_Order1 &o1, C_Order2 &o2) {
		CHECK(o1.i < o2.j);
	});

	// Add the test components
	ecs::add_component(0, C_Order1{ 1 });
	ecs::add_component(0, C_Order2{ 2 });

	ecs::update_systems();
}

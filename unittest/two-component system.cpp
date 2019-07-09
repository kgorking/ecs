#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Two-component system")
{
	SECTION("Test a system with two components")
	{
		ecs::runtime::reset();

		// Add a system for the C_Counter+C_Name component
		ecs::add_system([](int const& i, unsigned const& u) {
			REQUIRE(i == 1);
			REQUIRE(u == 2);
		});

		// Create an entity and add stuff to it
		ecs::entity_id const e = 0;
		ecs::add_component(e, int{ 1 });
		ecs::add_component(e, unsigned{ 2 });

		// Run the system
		ecs::update_systems();
	}
}

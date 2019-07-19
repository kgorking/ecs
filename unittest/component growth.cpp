#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Component growth")
{
	SECTION("Test that components are stored properly")
	{
		ecs::runtime::reset();

		// Add a system to verify the values of a component
		ecs::add_system([](ecs::entity_id ent, unsigned &c) {
			REQUIRE(ent.id == c);
		});

		for (auto e = 0u; e < 100; e++) {
			// Force a grow and check everytime a new component is added
			ecs::add_component(e, e);
			ecs::update_systems();
		}

		// Get the component data to verify
		for (auto e = 0u; e < 100; e++) {
			size_t const c = ecs::get_component<unsigned>(e);
			REQUIRE(e == c);
		}
	}
}

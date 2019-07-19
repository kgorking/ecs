#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Component deletion")
{
	SECTION("Test the deletion of components")
	{
		ecs::runtime::reset();

		// Add a system for unsigned components
		ecs::add_system([](ecs::entity_id id, unsigned const& c) {
			REQUIRE(id.id == c);
		});

		// Create some entities and add an unsigned component initialized to its index
		// 0 1 2 3 4 5 6 7 8 9 10
		for (unsigned e = 0; e <= 10; e++)
			ecs::add_component(e, e);

		// This should be zero, because changes hasn't been processed yet
		REQUIRE(0ull == ecs::get_component_count<unsigned>());

		// commit and run
		ecs::update_systems();

		// Verify the component count and data
		REQUIRE(11ull == ecs::get_component_count<unsigned>());

		// Remove some components
		// 0 1 2 3 4 10
		for (unsigned e = 9; e >= 5; --e)
			ecs::remove_component<unsigned>(e);
		ecs::update_systems();

		// Make sure components are deleted
		REQUIRE(6ull == ecs::get_component_count<unsigned>());

		// re-insert new stuff
		// 0 1 2 3 4 6 7 8 9 10
		for (unsigned e = 6; e <= 9; e++)
			ecs::add_component(e, e);
		ecs::update_systems();

		// There should now be 10 components
		REQUIRE(10ull == ecs::get_component_count<unsigned>());
	}
}

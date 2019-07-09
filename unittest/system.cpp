#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Simple system")
{
	SECTION("Test simple system")
	{
		// Add a system with a size_t component
		ecs::system & sys = ecs::add_system([](size_t &c) {
			c++;
		});

		// Create an entity and add a size_t component initialized to zero
		ecs::entity_id const e = 0;
		ecs::add_component(e, size_t{ 0 });
		ecs::commit_changes();

		REQUIRE(1 == ecs::get_component_count<size_t>());

		// Run the system 5 times
		for (int i = 0; i < 5; i++)
			sys.update();

		// Get the component data to verify that the system was run the correct number of times
		auto c = ecs::get_component<size_t>(e);
		REQUIRE(5u == c);
	}
}

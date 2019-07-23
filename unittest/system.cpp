#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Test simple system")
{
	// Add a system for the C_Counter component
	ecs::system &sys = ecs::add_system([](size_t &c) {
		c++;
	});

	// Create an entity and add a size_t component initialized to zero
	ecs::add_component(0, size_t{ 0 });
	ecs::commit_changes();

	CHECK(1 == ecs::get_component_count<size_t>());

	// Run the system 5 times
	for (int i = 0; i < 5; i++)
		sys.update();

	// Get the component data to verify that the system was run the correct number of times
	auto const c = ecs::get_component<size_t>(0);
	CHECK(5u == c);
}

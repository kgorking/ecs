#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Simple system", "[system]")
{
	// Add a system for the size_t component
	ecs::system &sys = ecs::add_system([](size_t &c) {
		c++;
	});

	// Add the component to an entity
	ecs::add_component(0, size_t{ 0 });
	ecs::commit_changes();

	CHECK(1 == ecs::get_component_count<size_t>());

	// Run the system 5 times
	for (int i = 0; i < 5; i++) {
		sys.update();
	}

	// Get the component data to verify that the system was run the correct number of times
	auto const c = *ecs::get_component<size_t>(0);
	CHECK(5U == c);
}

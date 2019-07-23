#include <ecs/ecs.h>
#include "catch.hpp"

// A bunch of tests to ensure that components are properly stored internally 
TEST_CASE("Test the internal storage of components")
{
	ecs::runtime::reset();

	// Add a system that verifies an unsigned matches its entity id
	int run_counter = 0;
	ecs::add_system([&run_counter](ecs::entity_id id, int const& c) {
		CHECK(id == c);
		run_counter++;
	});

	// Add components to some entities
	ecs::add_component_range_init(0, 10, [](auto ent) -> int { return ent.id; });

	// commit and run
	ecs::update_systems();

	// Verify the component count
	CHECK(11ull == ecs::get_component_count<int>());

	SECTION("Adding 11 components")
	{
		// Verify that the system was run 11 times
		CHECK(11 == run_counter);
	}

	SECTION("Remove 2 components from the back")
	{
		ecs::remove_component_range<int>(9, 10);
		ecs::update_systems();

		CHECK(9ull == ecs::get_component_count<int>());
		CHECK(11 + 9 == run_counter);
	}

	SECTION("Remove 2 components from the front")
	{
		ecs::remove_component_range<int>(0, 1);
		ecs::update_systems();

		CHECK(9ull == ecs::get_component_count<int>());
		CHECK(11 + 9 == run_counter);
	}

	SECTION("Remove 2 components from the middle")
	{
		ecs::remove_component_range<int>(4, 5);
		ecs::update_systems();

		CHECK(9ull == ecs::get_component_count<int>());
		CHECK(11 + 9 == run_counter);
	}
}

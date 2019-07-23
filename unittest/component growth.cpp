#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Test that components remain valid after memory reallocation")
{
	ecs::runtime::reset();

	// Add a system to verify the values of a component
	ecs::add_system([](ecs::entity_id ent, int const& c) {
		CHECK(ent == c);
	});

	// Add components to entities [0..9] and [20..29]
	ecs::add_component_range_init(0 , 2 , [](ecs::entity_id ent) { return ent.id; });
	ecs::add_component_range_init(6, 9, [](ecs::entity_id ent) { return ent.id; });
	ecs::commit_changes();

	for (int e = 5; e >= 3; e--) {
		// Force a grow
		ecs::add_component(e, e);
		ecs::commit_changes();
	}

	// Check the components
	ecs::run_systems();
}

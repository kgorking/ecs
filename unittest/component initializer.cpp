#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Test the use lambdas to initialize components")
{
	ecs::runtime::reset();

	// Add a system for unsigned components
	ecs::add_system([](ecs::entity_id ent, int const& c) {
		REQUIRE(ent == c);
		});

	ecs::add_component_range_init(0, 9, [](ecs::entity_id ent) {
		return ent.id;
	});

	ecs::update_systems();
}

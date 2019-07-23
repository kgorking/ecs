#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Test the use of negative entity ids")
{
	ecs::runtime::reset();

	// Add a system for unsigned components
	ecs::add_system([](ecs::entity_id id, int const& c) {
		CHECK(id == c);
	});

	ecs::add_component_range_init(-10, 10, [](ecs::entity_id ent) { return ent.id; });
	ecs::update_systems();

	CHECK(21 == ecs::get_component_count<int>());
}

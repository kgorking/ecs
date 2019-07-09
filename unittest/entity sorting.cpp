#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Entity sorting")
{
	SECTION("Test the internal sorting of entities")
	{
		ecs::runtime::reset();

		ecs::entity_id last = 0;
		ecs::add_system([&last](unsigned const& c) {
			REQUIRE(last < c);
			last = c;
		});

		ecs::add_component(4, unsigned{ 4 });
		ecs::add_component(1, unsigned{ 1 });
		ecs::add_component(2, unsigned{ 2 });
		ecs::update_systems();

		last = 0;
		ecs::add_component(9, unsigned{ 9 });
		ecs::add_component(3, unsigned{ 3 });
		ecs::add_component(7, unsigned{ 7 });
		ecs::update_systems();
	}
};

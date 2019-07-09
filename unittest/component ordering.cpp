#include <ecs/ecs.h>
#include "catch.hpp"

struct C_Order1 { unsigned i; };
struct C_Order2 { unsigned j; };

TEST_CASE("Component ordering")
{
	SECTION("Test that the components are passed in the correct order to the system")
	{
		ecs::runtime::reset();

		ecs::add_system([](ecs::entity_id ent, C_Order1 &o1, C_Order2 &o2) {
			REQUIRE(ent.id + 100 != o1.i); // Component order is reversed
			REQUIRE(ent.id == o1.i);

			REQUIRE(ent.id + 100 == o2.j);
			REQUIRE(o1.i < o2.j); // Component order is reversed
		});

		for (auto e = 0u; e < 1; e++)
		{
			ecs::add_component(e, C_Order1{ e });
			ecs::add_component(e, C_Order2{ e + 100 });
		}
		ecs::update_systems();
	}
}

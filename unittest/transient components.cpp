#include <ecs/ecs.h>
#include "catch.hpp"

struct C_TransientTest : ecs::transient {
	int i = 0;
};

TEST_CASE("Transient components")
{
	SECTION("Test of transient components")
	{
		ecs::runtime::reset();

		int counter = 0;
		ecs::add_system([&counter](C_TransientTest const&) {
			counter++;
		});

		ecs::add_component_range(0, 99, C_TransientTest{});

		ecs::update_systems();
		REQUIRE(100 == counter);

		// C_TransientTest will be removed in this update,
		// and the counter should thus not be incremented
		ecs::update_systems();
		REQUIRE(100 == counter);

		// No transient components should be active
		REQUIRE(0ull == ecs::get_component_count<C_TransientTest>());
	}
}

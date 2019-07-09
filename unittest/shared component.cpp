#include <ecs/ecs.h>
#include "catch.hpp"

struct C_SharedTest : ecs::shared {
	int i = 0;
};

TEST_CASE("Shared component")
{
	SECTION("Test shared components")
	{
		ecs::runtime::reset();

		auto pst = ecs::get_shared_component<C_SharedTest>();
		pst->i = 42;

		ecs::add_system([](C_SharedTest const& st) {
			REQUIRE(42 == st.i);
		});

		ecs::add_component_range(0, 256, C_SharedTest{});
		ecs::update_systems();

		REQUIRE(1 == ecs::get_component_count<C_SharedTest>());
	}
}

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

		ecs::add_component_range(0, 255, C_SharedTest{});
		ecs::commit_changes();

		// Only 1 C_SharedTest should exist
		REQUIRE(1 == ecs::get_component_count<C_SharedTest>());

		// Ensure that different entities have the same shared component
		ptrdiff_t const diff = &ecs::get_component<C_SharedTest>(4) - &ecs::get_component<C_SharedTest>(110);
		REQUIRE(diff == 0);

		// Test the content of the entities
		ecs::run_systems();
	}
}

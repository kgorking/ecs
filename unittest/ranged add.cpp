#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Ranged add")
{
	SECTION("Test of ranged add of components")
	{
		ecs::runtime::reset();
		ecs::runtime::init_components<size_t>();

		ecs::add_component_range(0, 5, size_t{ 5 });
		ecs::commit_changes();

		for (auto i = 0; i < 5; ++i) {
			size_t const& loc = ecs::get_component<size_t>(i);
			REQUIRE(loc == 5);
		}
	}

	SECTION("Test of ranged add of components with initializer")
	{
		ecs::runtime::reset();
		ecs::runtime::init_components<size_t>();

		ecs::add_component_range<size_t>(0, 5, [](auto ent) { return ent.id * 2ull; });

		ecs::entity_range rng{ 6,10 };
		rng.add<size_t>([](ecs::entity_id ent) { return ent.id * 2ull; });

		ecs::commit_changes();

		for (auto i = 0; i <= 10; ++i) {
			size_t const& loc = ecs::get_component<size_t>(i);
			REQUIRE(loc == i*2);
		}
	}
}

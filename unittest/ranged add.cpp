#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Ranged add")
{
	SECTION("Test of ranged add of components")
	{
		ecs::runtime::reset();
		ecs::runtime::init_components<size_t>();

		ecs::add_component_range({ 0, 5 }, size_t{ 5 });
		ecs::commit_changes();

		for (ecs::entity_id i = 0; i < 5; ++i) {
			size_t const& loc = ecs::get_component<size_t>(i);
			CHECK(loc == 5);
		}
	}

	SECTION("Test of ranged add of components with initializer")
	{
		ecs::runtime::reset();
		ecs::runtime::init_components<int>();

		auto const init = [](auto ent) { return ent.id * 2; };

		ecs::add_component_range_init({ 0, 5 }, init);

		ecs::entity_range rng{ 6,10 };
		rng.add_init(init);

		ecs::commit_changes();

		for (ecs::entity_id i = 0; i <= 10; ++i) {
			int const& loc = ecs::get_component<int>(i);
			CHECK(loc == i.id * 2);
		}
	}
}

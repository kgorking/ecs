#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Ranged add", "[range]")
{
	SECTION("Ranged add of components")
	{
		ecs::context::reset();
		ecs::context::init_components<size_t>();

		ecs::add_component({ 0, 5 }, size_t{ 5 });
		ecs::commit_changes();

		for (ecs::entity_id i = 0; i < 5; ++i) {
			size_t const& loc = ecs::get_component<size_t>(i);
			CHECK(loc == 5);
		}
	}

	SECTION("Ranged add of components with initializer")
	{
		ecs::context::reset();
		ecs::context::init_components<int>();

		auto const init = [](auto ent) { return ent.id * 2; };

		ecs::add_component({ 0, 5 }, init);
		ecs::entity_range{ 6, 10, init };

		ecs::commit_changes();

		for (ecs::entity_id i = 0; i <= 10; ++i) {
			int const& loc = ecs::get_component<int>(i);
			CHECK(loc == i.id * 2);
		}
	}
}

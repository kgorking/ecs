#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Ranged add", "[range]")
{
	SECTION("Ranged add of components")
	{
		ecs::detail::_context.reset();
		ecs::detail::_context.init_component_pools<size_t>();

		ecs::add_component({ 0, 5 }, size_t{ 5 });
		ecs::commit_changes();

		for (ecs::entity_id i = 0; i < 5; ++i) {
			size_t const& loc = *ecs::get_component<size_t>(i);
			CHECK(loc == 5);
		}
	}

	SECTION("Ranged add of components with initializer")
	{
		ecs::detail::_context.reset();
		ecs::detail::_context.init_component_pools<int>();

		auto const init = [](auto ent) { return int{ ent * 2 }; };

		ecs::add_component({ 0, 5 }, init);
		ecs::entity_range const ents{ 6, 10, init };

		ecs::commit_changes();

		for (ecs::entity_id i = 0; i <= 10; ++i) {
			int const& loc = *ecs::get_component<int>(i);
			CHECK(loc == i * 2);
		}
	}
}

#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Ranged add", "[range]")
{
	struct range_add {
		int i;
	};

	SECTION("Ranged add of components")
	{
		ecs::detail::_context.reset();
		ecs::detail::_context.init_component_pools<range_add>();

		ecs::add_component({ 0, 5 }, range_add{ 5 });
		ecs::entity_range const ents{ 6, 10, range_add{ 5 } };
		ecs::commit_changes();

		for (ecs::entity_id i = 0; i <= 10; ++i) {
			auto const& ra = *ecs::get_component<range_add>(i);
			CHECK(ra.i == 5);
		}
	}

	SECTION("Ranged add of components with initializer")
	{
		ecs::detail::_context.reset();
		ecs::detail::_context.init_component_pools<range_add>();

		auto const init = [](auto ent) -> range_add {
			return { ent * 2 };
		};

		ecs::add_component({ 0, 5 }, init);
		ecs::entity_range const ents{ 6, 10, init };

		ecs::commit_changes();

		for (ecs::entity_id i = 0; i <= 10; ++i) {
			auto const& ra = *ecs::get_component<range_add>(i);
			CHECK(ra.i == i*2);
		}
	}
}

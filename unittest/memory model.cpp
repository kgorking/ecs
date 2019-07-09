#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Memory model")
{
	SECTION("Ensure that unused memory is compacted")
	{
		ecs::runtime::reset();
		ecs::runtime::init_components<size_t>();

		// add 100 components
		ecs::add_component_range(0, 100, size_t{ 0 });
		ecs::commit_changes();

		// remove 98 components from the middle
		ecs::remove_component_range<size_t>(1, 99);
		ecs::commit_changes();

		// Find the distance between component 0 and 100
		size_t const& c0 = ecs::get_component<size_t>(0);
		size_t const& c100 = ecs::get_component<size_t>(100);
		ptrdiff_t const distance_between_components = &c100 - &c0;

		REQUIRE(distance_between_components == 1);
	}
}

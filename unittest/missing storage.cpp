#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Trying to use missing storage", "[storage]")
{
	ecs::detail::_context.reset();

	// Use a local struct to avoid it possibly
	// already existing from another unittest
	struct S { size_t c; };

	// Add a system-less component to an entity
	ecs::add_component(0, S{ 0 });
	ecs::commit_changes();
	REQUIRE(ecs::get_component_count<S>() == 1);
}

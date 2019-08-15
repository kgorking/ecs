#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Trying to use missing storage", "[storage]")
{
	ecs::runtime::reset();

	// Use a local struct to avoid it possibly
	// already existing from another unittest
	struct S { size_t c; };

	try
	{
		// Add a system-less component to an entity
		ecs::add_component(0, S{ 0 });
		FAIL("No exception caught!");
	}
	catch (std::exception const&)
	{
		SUCCEED();
	}
}

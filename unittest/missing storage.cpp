#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Missing storage")
{
	SECTION("Test response when trying to use missing storage")
	{
		ecs::runtime::reset();

		// Use a hidden local struct to avoid it possibly
		// already existing from another unittest
		struct S { size_t c; };

		try
		{
			// Add a system-less component to an entity
			ecs::add_component(0, S{ 0 });
			FAIL("No exception caught!");
		}
		catch (std::exception const& e)
		{
			std::string msg{ "\nMissing pool for '" };
			msg += e.what();
			msg += "'; make sure a system is added first";
			//INFO(msg);
			SUCCEED(msg);
		}
	}
}

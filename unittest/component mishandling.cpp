#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Mishandling components")
{
	SECTION("adding the same component twice")
	{
		ecs::runtime::reset();

		// Use a hidden local struct to ensure uniqueness
		struct A { size_t c; };

		ecs::runtime::init_components<A>();
		try
		{
			ecs::add_component(0, A{ 0 });
			ecs::add_component(0, A{ 1 });
			FAIL("No exception caught!");
		}
		catch (std::logic_error const& e)
		{
			SUCCEED(e.what());
		}
		catch (...)
		{
			FAIL("Unexpected exception caught!");
		}
	}

	SECTION("adding the same component to an entity that already has it")
	{
		ecs::runtime::reset();

		// Use a hidden local struct to ensure uniqueness
		struct A { size_t c; };

		ecs::runtime::init_components<A>();
		try
		{
			ecs::add_component(0, A{ 0 });
			ecs::commit_changes();
			ecs::add_component(0, A{ 1 });
			FAIL("No exception caught!");
		}
		catch (std::logic_error const& e)
		{
			SUCCEED(e.what());
		}
		catch (...)
		{
			FAIL("Unexpected exception caught!");
		}
	}

	SECTION("removing a non-existing component from an entity")
	{
		ecs::runtime::reset();

		// Use a hidden local struct to ensure uniqueness
		struct B { size_t c; };

		ecs::runtime::init_components<B>();
		try
		{
			ecs::remove_component<B>(0);
			FAIL("No exception caught!");
		}
		catch (std::logic_error const& e)
		{
			SUCCEED(e.what());
		}
		catch (...)
		{
			FAIL("Unexpected exception caught!");
		}
	}
}

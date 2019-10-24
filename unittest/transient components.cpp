#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Transient components", "[component][transient]")
{
	struct foo {};
	struct test_t : ecs::transient {};

	ecs::detail::_context.reset();

	int counter = 0;
	ecs::add_system([&counter](foo const&, test_t const&) {
		counter++;
	});

	ecs::add_component(0, test_t{});
	ecs::add_component(0, foo{});
	ecs::update_systems();
	CHECK(1 == counter);

	// test_t will be removed in this update,
	// and the counter should thus not be incremented
	ecs::update_systems();
	CHECK(1 == counter);

	// Add the test_t again, should increment the count
	ecs::add_component(0, test_t{});
	ecs::update_systems();
	CHECK(2 == counter);

	// This should clean up test_t
	ecs::commit_changes();

	// No transient components should be active
	CHECK(0ull == ecs::get_component_count<test_t>());
}

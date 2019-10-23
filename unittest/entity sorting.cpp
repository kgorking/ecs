#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Internal sorting of entities", "[component]")
{
	ecs::context::reset();

	int last = 0;
	ecs::add_system([&last](int const& c) {
		CHECK(last < c);
		last = c;
	});

	ecs::add_component(4, 4);
	ecs::add_component(1, 1);
	ecs::add_component(2, 2);
	ecs::update_systems();

	last = 0;
	ecs::add_component(9, 9);
	ecs::add_component(3, 3);
	ecs::add_component(7, 7);
	ecs::update_systems();
};

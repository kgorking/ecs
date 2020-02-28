#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Internal sorting of entities", "[component]")
{
	ecs::detail::_context.reset();

	struct ent_sort {
		int c;
	};
	int last = 0;
	ecs::make_system([&last](ent_sort const& es) {
		CHECK(last < es.c);
		last = es.c;
	});

	ecs::add_component(4, ent_sort{ 4 });
	ecs::add_component(1, ent_sort{ 1 });
	ecs::add_component(2, ent_sort{ 2 });
	ecs::update_systems();

	last = 0;
	ecs::add_component(9, ent_sort{ 9 });
	ecs::add_component(3, ent_sort{ 3 });
	ecs::add_component(7, ent_sort{ 7 });
	ecs::update_systems();
};

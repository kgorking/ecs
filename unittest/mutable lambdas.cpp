#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Mutable lambdas are supported", "[system]")
{
	ecs::context::reset();

	// Add some systems to test
	ecs::add_system([counter = 0](int &i) mutable
	{
		i = counter++;
	});
	ecs::add_system([](ecs::entity_id ent, int const& i) {
		CHECK(ent.id == i);
	});

	// Create 100 entities and add stuff to them
	ecs::add_component({ 0, 3 }, int{ 0 });
	ecs::update_systems();
}

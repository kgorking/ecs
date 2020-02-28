#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Mutable lambdas are supported", "[system]")
{
	ecs::detail::_context.reset();

	struct  mut_lambda {
		int i;
	};

	// Add some systems to test
	ecs::make_system([counter = 0](mut_lambda& ml) mutable
	{
		ml.i = counter++;
	});
	ecs::make_system([](ecs::entity_id ent, mut_lambda const& ml) {
		CHECK(ent == ml.i);
	});

	// Create 100 entities and add stuff to them
	ecs::add_component({ 0, 3 }, mut_lambda{ 0 });
	ecs::update_systems();
}

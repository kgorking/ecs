#include <ecs/ecs.h>
#include <catch2/catch_test_macros.hpp>

struct _dummy {
	using ecs_flags = ecs::flags<ecs::transient>;
};

TEST_CASE("Transient components", "[component][transient]") {
	ecs::runtime ecs;

	struct foo {};
	struct test_t {
		using ecs_flags = ecs::flags<ecs::transient>;
	};

	int counter = 0;
	ecs.make_system<ecs::opts::not_parallel>([&counter](foo const& /*f*/, test_t const& /*t*/) { counter++; });

	ecs.add_component(0, test_t{});
	ecs.add_component(0, foo{});
	ecs.update();
	CHECK(1 == counter);

	// test_t will be removed in this update,
	// and the counter should thus not be incremented
	ecs.update();
	CHECK(1 == counter);

	// Add the test_t again, should increment the count
	ecs.add_component(0, test_t{});
	ecs.update();
	CHECK(2 == counter);

	// This should clean up test_t
	ecs.commit_changes();

	// No transient components should be active
	CHECK(0ULL == ecs.get_component_count<test_t>());
}

#include <ecs/ecs.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("System options tests") {
	SECTION("opts::manual_update works correctly") {
		ecs::runtime ecs;

		int counter = 0;
		auto& test_sys = ecs.make_system<ecs::opts::manual_update>([&counter](short const&) { counter++; });

		ecs.add_component(0, short{});

		ecs.update(); // will not call 'test_sys.run()'
		REQUIRE(counter == 0);

		test_sys.run();
		REQUIRE(counter == 1);
	}
}

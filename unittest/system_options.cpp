#include <ecs/ecs.h>
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("System options tests") {
	SECTION("opts::group<> order systems correctly") {
		ecs::runtime ecs;

		struct S1 {};
		struct S2 {};
		struct S3 {};
		struct Sx {};

		// Add systems in reverse order, they should execute in correct order
		int counter = 0;
		ecs.make_system<ecs::opts::group<3>>([&counter](S3&) {
			REQUIRE(counter == 3);
			counter++;
		});
		ecs.make_system<ecs::opts::group<2>>([&counter](S2&) {
			REQUIRE(counter == 2);
			counter++;
		});
		ecs.make_system<ecs::opts::group<1>>([&counter](S1&) {
			REQUIRE(counter == 1);
			counter++;
		});
		ecs.make_system<ecs::opts::group<0>>([&counter](Sx&) {
			REQUIRE(counter == 0);
			counter++;
		});

		ecs.add_component(0, S1{}, S3{}, Sx{}, S2{});
		ecs.update();
	}

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

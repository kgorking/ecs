#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("System specification", "[system]") {
	SECTION("Running a system works") {
		struct local {
			int c;
		};
		// Add a system for the local component
		ecs::system& sys = ecs::make_system([](local& l) {
			l.c++;
		});

		// Add the component to an entity
		ecs::add_component(0, local{ 0 });
		ecs::commit_changes();

		// Run the system 5 times
		for (int i = 0; i < 5; i++) {
			sys.update();
		}

		// Get the component data to verify that the system was run the correct number of times
		auto const l = *ecs::get_component<local>(0);
		REQUIRE(5U == l.c);
	}

	SECTION("Verify enable/disable functions") {
		struct local {};
		ecs::system& sys = ecs::make_system([](local const& c) {});

		REQUIRE(true == sys.is_enabled());
		sys.disable();
		REQUIRE(false == sys.is_enabled());
		sys.enable();
		REQUIRE(true == sys.is_enabled());
		sys.set_enable(false);
		REQUIRE(false == sys.is_enabled());
	}

	SECTION("Disabling systems prevents them from running") {
		struct local {
			int c;
		};
		// Add a system for the local component
		ecs::system& sys = ecs::make_system([](local& l) {
			l.c++;
		});

		ecs::add_component(0, local{ 0 });
		ecs::commit_changes();

		// Run the system and check value
		sys.update();
		REQUIRE(1 == ecs::get_component<local>(0)->c);

		// Disable system and re-run. Should not change the component
		sys.disable();
		sys.update();
		REQUIRE(1 == ecs::get_component<local>(0)->c);

		// Enable system and re-run. Should change the component
		sys.enable();
		sys.update();
		REQUIRE(2 == ecs::get_component<local>(0)->c);
	}

	SECTION("Re-enabling systems forces a rebuild") {
		struct local {
			int c;
		};
		// Add a system for the local component
		ecs::system& sys = ecs::make_system([](local& l) {
			l.c++;
		});
		sys.disable();

		ecs::add_component(0, local{ 0 });
		ecs::commit_changes();
		sys.update();
		REQUIRE(0 == ecs::get_component<local>(0)->c);

		sys.enable();
		sys.update();
		REQUIRE(1 == ecs::get_component<local>(0)->c);
	}

	SECTION("Groups order systems correctly") {
		struct S1 {};
		struct S2 {};
		struct S3 {};
		struct Sx {};

		// Add systems in reverse order, they should execute in correct order
		int counter = 0;
		ecs::make_system<                              3>([&counter](S3&) { REQUIRE(counter == 3); counter++; });
		ecs::make_system<                              2>([&counter](S2&) { REQUIRE(counter == 2); counter++; });
		ecs::make_system<                              1>([&counter](S1&) { REQUIRE(counter == 1); counter++; });
		ecs::make_system<std::numeric_limits<int>::min()>([&counter](Sx&) { REQUIRE(counter == 0); counter++; });

		ecs::entity const e{ 0, S1{}, S3{}, Sx{}, S2{} };
		ecs::update_systems();
	}
}

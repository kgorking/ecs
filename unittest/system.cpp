#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("System specification", "[system]") {
	SECTION("Running a system works") {
		// Add a system for the size_t component
		ecs::system& sys = ecs::add_system([](size_t& c) {
			c++;
		});

		// Add the component to an entity
		ecs::add_component(0, size_t{ 0 });
		ecs::commit_changes();

		// Run the system 5 times
		for (int i = 0; i < 5; i++) {
			sys.update();
		}

		// Get the component data to verify that the system was run the correct number of times
		size_t const c = *ecs::get_component<size_t>(0);
		REQUIRE(5U == c);
	}

	SECTION("Verify enable/disable functions") {
		ecs::system& sys = ecs::add_system([](float& c) {});
		REQUIRE(true == sys.is_enabled());
		sys.disable();
		REQUIRE(false == sys.is_enabled());
		sys.enable();
		REQUIRE(true == sys.is_enabled());
		sys.set_enable(false);
		REQUIRE(false == sys.is_enabled());
	}

	SECTION("Disabling systems prevents them from running") {
		ecs::system& sys = ecs::add_system([](int& c) {
			c++;
		});

		ecs::add_component(0, int{ 0 });
		ecs::commit_changes();

		// Run the system and check value
		sys.update();
		REQUIRE(1 == *ecs::get_component<int>(0));

		// Disable system and re-run. Should not change the component
		sys.disable();
		sys.update();
		REQUIRE(1 == *ecs::get_component<int>(0));

		// Enable system and re-run. Should change the component
		sys.enable();
		sys.update();
		REQUIRE(2 == *ecs::get_component<int>(0));
	}
}

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <atomic>
#include <ecs/ecs.h>

template <size_t I>
struct type {};

// Tests to make sure the scheduler works as intended.
// Test are done with operator precedence, so tests run
// in the wrong order will produce the wrong result
// ie. (0+1)*100 instead of (0*100)+1
TEST_CASE("Scheduler") {
	SECTION("no dependency") {
		ecs::runtime ecs;

		ecs.make_system([](int& i) {
			i += 1;
		});

		ecs.add_component({0, 9}, int{0});
		for (int i = 0; i < 10; i++) {
			ecs.update();
		}

		for (int const i : ecs.get_components<int>({0, 9}))
			REQUIRE(i == 10);
	}

	SECTION("simple dependency") {
		ecs::runtime ecs;

		ecs.make_system([](int& i) {
			i += 1;
		});
		ecs.make_system([](int& i) {
			i *= 100;
		});

		ecs.add_component({0, 9}, int{0});
		ecs.update();

		for (int const i : ecs.get_components<int>({0, 9}))
			REQUIRE(i == 100);
	}

	SECTION("double dependency") {
		ecs::runtime ecs;

		ecs.make_system([](int& i) {
			i += 1;
		});
		ecs.make_system([](long& l) {
			l += 10;
		});
		ecs.make_system([](int& i, long& l) {
			i *= 2;
			l *= 2;
		});

		ecs.add_component({0, 9}, long{0}, int{0});
		ecs.update();

		for (int const i : ecs.get_components<int>({0, 9}))
			REQUIRE(i == 2);
		for (long const i : ecs.get_components<long>({0, 9}))
			REQUIRE(i == 20);
	}

	SECTION("partial dependency") {
		ecs::runtime ecs;

		ecs.make_system([](int& i) {
			i += 1;
		});
		ecs.make_system([](long& l) {
			l += 10;
		});
		ecs.make_system([](int& i, long& l) {
			i *= 2;
			l *= 2;
		});
		ecs.make_system([](int& i, long*) {
			i *= -1;
		});

		ecs.add_component({0, 9}, int{0});
		ecs.add_component({4, 9}, long{0});
		ecs.update();

		for (int const i : ecs.get_components<int>({0, 3}))
			REQUIRE(i == -1);
		for (int const i : ecs.get_components<int>({4, 9}))
			REQUIRE(i == 2);
		for (long const i : ecs.get_components<long>({4, 9}))
			REQUIRE(i == 20);
	}

	SECTION("many-to-one dependency") {
		ecs::runtime ecs;

		for (int i = 0; i < 2; i++)
			ecs.make_system([](int& i) {
				i += 1;
			});
		for (int i = 0; i < 2; i++)
			ecs.make_system([](long& l) {
				l += 10;
			});

		ecs.make_system([](int& i, long& l) {
			i *= 2;
			l *= 2;
		});

		ecs.add_component({0, 9}, int{0}, long{0});
		ecs.update();

		for (int const i : ecs.get_components<int>({0, 9}))
			REQUIRE(i == 4);
		for (long const i : ecs.get_components<long>({0, 9}))
			REQUIRE(i == 40);
	}

	SECTION("one-to-many dependency") {
		ecs::runtime ecs;

		ecs.make_system([](int& i, long& l) {
			i += 1;
			l += 10;
		});

		for (int i = 0; i < 2; i++)
			ecs.make_system([](int& i) {
				i *= 2;
			});
		for (int i = 0; i < 2; i++)
			ecs.make_system([](long& l) {
				l *= 2;
			});

		ecs.add_component({0, 9}, int{0}, long{0});
		ecs.update();

		for (int const i : ecs.get_components<int>({0, 9}))
			REQUIRE(i == 4);
		for (long const i : ecs.get_components<long>({0, 9}))
			REQUIRE(i == 40);
	}

	SECTION("many-to-one-to-many dependency") {
		ecs::runtime ecs;

		for (int i = 0; i < 2; i++)
			ecs.make_system([](int& i) {
				i += 1;
			});
		for (int i = 0; i < 2; i++)
			ecs.make_system([](long& l) {
				l += 10;
			});

		ecs.make_system([](int& i, long& l) {
			i *= 2;
			l *= 2;
		});

		for (int i = 0; i < 2; i++)
			ecs.make_system([](int& i) {
				i -= 1;
			});
		for (int i = 0; i < 2; i++)
			ecs.make_system([](long& l) {
				l -= 1;
			});

		ecs.add_component({0, 9}, int{0}, long{0});
		ecs.update();

		for (int const i : ecs.get_components<int>({0, 9}))
			REQUIRE(i == 2);
		for (long const i : ecs.get_components<long>({0, 9}))
			REQUIRE(i == 38);
	}
}

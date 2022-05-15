#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>

TEST_CASE("System specification", "[system]") {
	SECTION("Running a system works") {
		ecs::runtime ecs;

		struct local1 {
			int c;
		};
		// Add a system for the local component
		auto& sys = ecs.make_system<ecs::opts::manual_update>([](local1& l) { l.c++; });

		// Add the component to an entity
		ecs.add_component(0, local1{0});
		ecs.commit_changes();

		// Run the system 5 times
		for (int i = 0; i < 5; i++) {
			sys.run();
		}

		// Get the component data to verify that the system was run the correct number of times
		auto const l = *ecs.get_component<local1>(0);
		REQUIRE(5U == l.c);
	}

	SECTION("Verify enable/disable functions") {
		ecs::runtime ecs;

		struct local2 {};
		auto& sys = ecs.make_system<ecs::opts::manual_update>([](local2 const& /*c*/) {});

		REQUIRE(true == sys.is_enabled());
		sys.disable();
		REQUIRE(false == sys.is_enabled());
		sys.enable();
		REQUIRE(true == sys.is_enabled());
		sys.set_enable(false);
		REQUIRE(false == sys.is_enabled());
	}

	SECTION("Disabling systems prevents them from running") {
		ecs::runtime ecs;

		struct local3 {
			int c;
		};
		// Add a system for the local component
		auto& sys = ecs.make_system<ecs::opts::manual_update>([](local3& l) { l.c++; });

		ecs.add_component(0, local3{0});
		ecs.commit_changes();

		// Run the system and check value
		sys.run();
		REQUIRE(1 == ecs.get_component<local3>(0)->c);

		// Disable system and re-run. Should not change the component
		sys.disable();
		sys.run();
		REQUIRE(1 == ecs.get_component<local3>(0)->c);

		// Enable system and re-run. Should change the component
		sys.enable();
		sys.run();
		REQUIRE(2 == ecs.get_component<local3>(0)->c);
	}

	SECTION("Re-enabling systems forces a rebuild") {
		ecs::runtime ecs;

		struct local4 {
			int c;
		};
		// Add a system for the local component
		auto& sys = ecs.make_system<ecs::opts::manual_update>([](local4& l) { l.c++; });
		sys.disable();

		ecs.add_component(0, local4{0});
		ecs.commit_changes();
		sys.run();
		REQUIRE(0 == ecs.get_component<local4>(0)->c);

		sys.enable();
		sys.run();
		REQUIRE(1 == ecs.get_component<local4>(0)->c);
	}

	SECTION("Read/write info on systems is correct") {
		ecs::runtime ecs;

		auto const& sys1 = ecs.make_system<ecs::opts::manual_update>([](int const&, float const&) {});
		CHECK(false == sys1.writes_to_component(ecs::detail::get_type_hash<int>()));
		CHECK(false == sys1.writes_to_component(ecs::detail::get_type_hash<float>()));

		auto const& sys2 = ecs.make_system<ecs::opts::manual_update>([](int&, float const&) {});
		CHECK(true == sys2.writes_to_component(ecs::detail::get_type_hash<int>()));
		CHECK(false == sys2.writes_to_component(ecs::detail::get_type_hash<float>()));

		auto const& sys3 = ecs.make_system<ecs::opts::manual_update>([](int&, float&) {});
		CHECK(true == sys3.writes_to_component(ecs::detail::get_type_hash<int>()));
		CHECK(true == sys3.writes_to_component(ecs::detail::get_type_hash<float>()));
	}

	SECTION("System with all combinations of types works") {
		ecs::runtime ecs;

		struct tagged {
			ecs_flags(ecs::flag::tag);
		};
		struct transient {
			ecs_flags(ecs::flag::transient);
		};
		struct immutable {
			ecs_flags(ecs::flag::immutable);
		};
		struct global {
			ecs_flags(ecs::flag::global);
		};

		auto constexpr vanilla_sort = [](int l, int r) {
			return l < r;
		};

		int last = -100'000'000;
		int run_counter = 0;
		ecs.make_system<ecs::opts::not_parallel>(
			[&](int const& v, tagged, transient const&, immutable const&, global const&, short*) {
				CHECK(last <= v);
				last = v;

				run_counter++;
			},
			vanilla_sort);

		std::vector<int> ints(1001);
		std::iota(ints.begin(), ints.end(), 0);

		ecs.add_component_span({0, 1000}, ints);
		ecs.add_component({0, 1000}, tagged{}, transient{}, immutable{});
		ecs.add_component({10, 20}, short{0});

		ecs.update();
		CHECK(run_counter == 1001 - 11);

		last = -100'000'000;
		ecs.update(); // transient component is gone, so system wont run
		CHECK(run_counter == 1001 - 11);
	}
	SECTION("Adding components during a system run works") {
		// Added this test in response to a bug found by https://github.com/relick

		ecs::runtime ecs;

		struct local5 {
			int c;
		};

		// Add a system for the local component
		ecs.make_system([&ecs](int const&) { ecs.add_component(0, local5{5}); });

		// Add an int component to trigger the system
		ecs.add_component(0, int{0});
		ecs.update();

		// Verify that the local component was added
		ecs.commit_changes();
		CHECK(size_t{1} == ecs.get_component_count<local5>());
	}
}

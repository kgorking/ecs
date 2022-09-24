#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>

TEST_CASE("Global components", "[component][global]") {
	SECTION("work in global systems") {
		ecs::runtime ecs;

		struct G1 {
			ecs_flags(ecs::flag::global);
			int i = 1;
		};
		struct G2 {
			ecs_flags(ecs::flag::global);
			int i = 2;
		};

		std::atomic_int runs = 0;
		ecs.make_system([&runs](G1 g1, G2 g2) {
			CHECK(g1.i == 1);
			CHECK(g2.i == 2);

			runs++;
		});

		ecs.update();
		CHECK(runs == 1);

		ecs.update();
		CHECK(runs == 2);

		ecs.add_component({0, 99}, int{}); // should have no effect
		ecs.update();
		CHECK(runs == 3);
	}

	SECTION("work in regular systems") {
		ecs::runtime ecs;

		struct test_s {
			ecs_flags(ecs::flag::global);
			int i = 0;
		};

		auto& pst = ecs.get_global_component<test_s>();
		pst.i = 42;

		int counter = 0;
		ecs.make_system<ecs::opts::not_parallel>([&counter](test_s const& st, int) {
			CHECK(42 == st.i);
			counter++;
		});

		ecs.add_component({0, 2}, int{});
		ecs.commit_changes();

		// Only 1 test_s should exist
		CHECK(size_t{1} == ecs.get_component_count<test_s>());

		// Test the content of the entities
		ecs.run_systems();
		CHECK(3 == counter);

		ecs.remove_component<int>({0, 2});
		ecs.commit_changes();
		ecs.run_systems();
		CHECK(3 == counter);
	}
}

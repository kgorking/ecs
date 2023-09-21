#include <ecs/ecs.h>
#include <atomic>
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("Filtering", "[component][system]") {
	ecs::runtime ecs;
	using ecs::opts::not_parallel;

	ecs.add_component({0, 6}, int());
	ecs.add_component({3, 9}, float());
	ecs.commit_changes();

	ecs.make_system([](ecs::entity_id id, int&) {
		CHECK(id >= 0);
		CHECK(id <= 6);
	});
	ecs.make_system([](ecs::entity_id id, float&) {
		CHECK(id >= 3);
		CHECK(id <= 9);
	});
	ecs.make_system([](ecs::entity_id id, int&, float*) {
		CHECK(id >= 0);
		CHECK(id <= 2);
	});
	ecs.make_system([](ecs::entity_id id, int*, float&) {
		CHECK(id >= 7);
		CHECK(id <= 9);
	});
	ecs.make_system([](ecs::entity_id id, int&, float&) {
		CHECK(id >= 3);
		CHECK(id <= 6);
	});

	// Filtering on non-existant component should run normally
	std::atomic_ptrdiff_t no_shorts = 0;
	ecs.make_system([&no_shorts](int&, short*) { no_shorts++; });

	ecs.run_systems();

	CHECK(no_shorts == ecs.get_entity_count<int>());
}

TEST_CASE("Filtering regression") {
	SECTION("empty pools when adding filtered system") {
		ecs::runtime rt;
		rt.add_component({0, 20}, int());
		rt.add_component({3, 9}, float());
		rt.add_component({14, 18}, short());

		rt.make_system([](int&, float*, short*) {});
		rt.update();
		SUCCEED();
	}

	SECTION("empty filters when adding filtered system") {
		ecs::runtime rt;
		rt.add_component({0, 20}, int());
		rt.commit_changes();

		rt.add_component({3, 9}, float());
		rt.add_component({14, 18}, short());

		rt.make_system([](int&, float*, short*) {});
		rt.update();
		SUCCEED();
	}

	SECTION("one empty filters when adding filtered system") {
		ecs::runtime rt;
		rt.add_component({0, 20}, int());
		rt.add_component({3, 9}, float());
		rt.commit_changes();

		rt.add_component({14, 18}, short());

		rt.make_system([](int&, float*, short*) {});
		rt.update();
		SUCCEED();
	}
}
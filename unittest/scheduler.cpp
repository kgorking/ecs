#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <atomic>
#include <ecs/ecs.h>

using namespace std::chrono_literals;

template <size_t I>
struct type {};

// Tests to make sure the scheduler works as intended.
TEST_CASE("Scheduler") {
	SECTION("verify wide dependency chains work") {
		ecs::runtime ecs;

		struct sched_test {};

		// The lambda to execute in the 100 systems
		std::atomic_int counter = 0;
		auto const incrementor = [&](sched_test const&) { counter++; };

		// The lambda to run after the 100 systems. Has a dependency on 'lambda'
		std::atomic_int num_checks = 0;
		auto const checker = [&](sched_test&) { num_checks++; };

		// Create 100 systems that will execute concurrently,
		// because they have no dependencies on each other.
		for (int i = 0; i < 100; i++) {
			ecs.make_system(incrementor);
		}

		// Create a system that will only run after the 100 systems.
		// It can not run concurrently with the other 100 systems,
		// because it has a read/write dependency on all 100 systems.
		ecs.make_system(checker);

		// Add a component to trigger the systems
		ecs.add_component(0, sched_test{});
		ecs.commit_changes();

		// Run it 500 times
		for (int i = 0; i < 500; i++) {
			ecs.run_systems();
			CHECK(100 == counter);
			CHECK(1 == num_checks);
			counter = 0;
			num_checks = 0;
		}
	}

	SECTION("Correct concurrency") {
		ecs::runtime ecs;

		std::atomic_int sys1 = 0;
		std::atomic_int sys2 = 0;
		std::atomic_int sys3 = 0;
		std::atomic_int sys4 = 0;
		std::atomic_int sys5 = 0;
		std::atomic_int sys6 = 0;

		constexpr int num_entities = 1024*256;

		ecs.make_system([&sys1](type<0>&, type<1> const&) { ++sys1; });

		ecs.make_system([&sys2, &sys1](type<1>&) {
			CHECK(sys1 == num_entities);
			++sys2;
		});

		ecs.make_system([&sys3](type<2>&) {
			++sys3;
		});

		ecs.make_system([&sys4, &sys1, &sys3](type<0> const&) {
			CHECK(sys1 == num_entities);
			++sys4;
		});

		ecs.make_system([&sys5, &sys3, &sys1](type<2>&, type<0> const&) {
			CHECK(sys3 == num_entities);
			CHECK(sys1 == num_entities);
			++sys5;
		});

		ecs.make_system([&sys6, &sys5](type<2> const&) {
			CHECK(sys5 == num_entities);
			++sys6;
		});

		// test on a bunch of entities
		ecs.add_component({1, num_entities}, type<0>{}, type<1>{}, type<2>{});
		ecs.update();

		CHECK(sys1 == num_entities);
		CHECK(sys2 == num_entities);
		CHECK(sys3 == num_entities);
		CHECK(sys4 == num_entities);
		CHECK(sys5 == num_entities);
		CHECK(sys6 == num_entities);
	}
}

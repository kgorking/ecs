#include <ecs/ecs.h>
#include <thread>
#include <atomic>
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

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
		auto const incrementor = [&](sched_test const&) {
			counter++;
		};

		// The lambda to run after the 100 systems. Has a dependency on 'lambda'
		std::atomic_int num_checks = 0;
		auto const checker = [&](sched_test&) {
			num_checks++;
		};

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

		std::atomic_bool sys1 = false;
		std::atomic_bool sys2 = false;
		std::atomic_bool sys3 = false;
		std::atomic_bool sys4 = false;
		std::atomic_bool sys5 = false;
		std::atomic_bool sys6 = false;

		ecs.make_system([&sys1](type<0>&, type<1> const&) {
			sys1 = true;
		});

		ecs.make_system([&sys2, &sys1](type<1>&) {
			CHECK(sys1 == true);
			sys2 = true;
		});

		ecs.make_system([&sys3](type<2>&) {
			std::this_thread::sleep_for(20ms);
			sys3 = true;
		});

		ecs.make_system([&sys4, &sys1, &sys3](type<0> const&) {
			CHECK(sys3 == false);
			CHECK(sys1 == true);
			sys4 = true;
		});

		ecs.make_system([&sys5, &sys3, &sys1](type<2>&, type<0> const&) {
			CHECK(sys3 == true);
			CHECK(sys1 == true);
			sys5 = true;
		});

		ecs.make_system([&sys6, &sys5](type<2> const&) {
			CHECK(sys5 == true);
			sys6 = true;
		});

		ecs.add_component(0, type<0>{}, type<1>{}, type<2>{});
		ecs.update();

		CHECK(sys1 == true);
		CHECK(sys2 == true);
		CHECK(sys3 == true);
		CHECK(sys4 == true);
		CHECK(sys5 == true);
		CHECK(sys6 == true);
	}
}

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>
#include <memory_resource>
#include <string>

struct ctr_counter {
	inline static size_t def_ctr_count = 0;
	inline static size_t ctr_count = 0;
	inline static size_t copy_count = 0;
	inline static size_t move_count = 0;
	inline static size_t dtr_count = 0;

	ctr_counter() noexcept {
		def_ctr_count++;
		ctr_count++;
	}
	ctr_counter(ctr_counter const& /*other*/) {
		copy_count++;
		ctr_count++;
	}
	ctr_counter(ctr_counter&& /*other*/) noexcept {
		move_count++;
		ctr_count++;
	}
	~ctr_counter() {
		dtr_count++;
	}

	ctr_counter& operator=(ctr_counter&&) = default;
	ctr_counter& operator=(ctr_counter const&) = default;
};

// A bunch of tests to ensure that the component_pool behaves as expected
TEST_CASE("Component pool specification", "[component]") {
	SECTION("A new component pool is empty") {
		ecs::detail::component_pool<int> pool;
		CHECK(pool.num_entities() == 0);
		CHECK(pool.num_components() == 0);
		CHECK(pool.has_component_count_changed() == false);
	}

	SECTION("An empty pool") {
		ecs::detail::component_pool<int> pool;

		// It won't throw, it will terminate
		/*SECTION("does not throw on bad remove") {
			pool.remove(0);
			pool.process_changes();
			SUCCEED();
		}*/
		SECTION("does not throw on bad component access") {
			CHECK(nullptr == pool.find_component_data(0));
		}
		SECTION("grows when data is added to it") {
			pool.add({0, 4}, 0);
			pool.process_changes();

			CHECK(pool.num_entities() == 5);
			CHECK(pool.num_components() == 5);
			CHECK(pool.has_more_components());
		}
	}

	SECTION("Adding components") {
		SECTION("does not perform unneccesary copies of components") {
			ecs::detail::component_pool<ctr_counter> pool;
			pool.add({0, 2}, ctr_counter{});
			pool.process_changes();
			pool.remove_range({0, 2});
			pool.process_changes();

			CHECK(ctr_counter::copy_count == 3);
			CHECK(ctr_counter::ctr_count == ctr_counter::dtr_count);
		}
		SECTION("with a lambda is valid") {
			ecs::detail::component_pool<int> pool;
			pool.add_init({0, 9}, [](ecs::entity_id ent) { return int{ent}; });
			pool.process_changes();

			for (int i = 0; i <= 9; i++) {
				CHECK(i == *pool.find_component_data(i));
			}
		}
		SECTION("with negative entity ids is fine") {
			ecs::detail::component_pool<int> pool;
			pool.add({-999, -950}, 0);
			pool.process_changes();

			CHECK(50 == pool.num_components());
			CHECK(50 == pool.num_entities());
		}
		SECTION("to previously deleted entities works (T1 -> T1)") {
			ecs::detail::component_pool<int> pool;
			pool.add({0, 9}, int{});
			pool.process_changes();

			// This creates a tier 1 memory block internally
			pool.remove_range({7, 9});
			pool.process_changes();

			// grow the t1 block
			pool.add({7, 8}, int{1});
			pool.process_changes();
		}
		SECTION("to previously deleted entities works (T1 -> T2)") {
			ecs::detail::component_pool<int> pool;
			pool.add({0, 9}, int{});
			pool.process_changes();

			// 0-6
			pool.remove_range({7, 9});
			pool.process_changes();

			// 0-6, 8-9
			pool.add({8, 9}, int{2});
			pool.process_changes();

			CHECK(9 == pool.num_components());
			CHECK(9 == pool.num_entities());

			CHECK(nullptr == pool.find_component_data(7));

			auto const result = pool.get_entities();
			REQUIRE(2 == result.size());
			CHECK(ecs::entity_range{0, 6}.equals(result[0]));
			CHECK(ecs::entity_range{8, 9}.equals(result[1]));
		}
		SECTION("to previously deleted entities works") {
			ecs::detail::component_pool<int> pool;
			pool.add({0, 9}, int{});
			pool.process_changes();

			pool.remove_range({3, 7});
			pool.process_changes();

			pool.add({4, 5}, int{});
			pool.process_changes();

			CHECK(7 == pool.num_components());
			CHECK(7 == pool.num_entities());

			CHECK(nullptr == pool.find_component_data(3));
			CHECK(nullptr == pool.find_component_data(7));

			auto const result = pool.get_entities();
			REQUIRE(3 == result.size());
			CHECK(ecs::entity_range{0, 2}.equals(result[0]));
			CHECK(ecs::entity_range{4, 5}.equals(result[1]));
			CHECK(ecs::entity_range{8, 9}.equals(result[2]));
		}
	}

	SECTION("Removing components") {
		ecs::detail::component_pool<int> pool;
		pool.add_init({0, 10}, [](auto ent) { return int{ent}; });
		pool.process_changes();

		SECTION("from the back does not invalidate other components") {
			pool.remove_range({9, 10});
			pool.process_changes();

			REQUIRE(pool.num_components() == 9);
			for (int i = 0; i <= 8; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}
		}
		SECTION("from the front does not invalidate other components") {
			pool.remove_range({0, 1});
			pool.process_changes();

			REQUIRE(pool.num_components() == 9);
			for (int i = 2; i <= 10; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}
		}
		SECTION("from the middle does not invalidate other components") {
			pool.remove_range({4, 5});
			pool.process_changes();

			REQUIRE(pool.num_components() == 9);
			for (int i = 0; i <= 3; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}
			for (int i = 6; i <= 10; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}
		}
		SECTION("piecewise works (T1)") {
			pool.remove_range({1, 10}); // moves mem to T1
			pool.process_changes();
			pool.remove_range({0, 0});  // should remove it
			pool.process_changes();

			REQUIRE(pool.num_components() == 0);
		}
		SECTION("piecewise works (T2)") {
			pool.remove_range({1, 4});
			pool.remove_range({6, 9}); // moves mem to T2
			pool.process_changes();

			pool.remove_range({5, 5}); // merges two previous skip chains
			pool.process_changes();

			pool.remove_range({10, 10});
			pool.process_changes();

			pool.remove_range({0, 0}); // should remove it
			pool.process_changes();

			REQUIRE(pool.num_components() == 0);
		}
		SECTION("piecewise does not invalidate other components") {
			pool.remove_range({10, 10});
			pool.remove_range({9, 9});
			pool.process_changes();

			REQUIRE(pool.num_components() == 9);
			for (int i = 0; i <= 8; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}
		}
	}

	SECTION("A non empty pool") {
		ecs::detail::component_pool<int> pool;
		pool.add_init({0, 9}, [](auto ent) { return int{ent}; });
		pool.process_changes();

		SECTION("has the correct entities") {
			REQUIRE(10 == pool.num_entities());
			REQUIRE(pool.has_entity({0, 9}));
		}
		SECTION("has the correct components") {
			REQUIRE(10 == pool.num_components());
			for (int i = 0; i <= 9; i++) {
				REQUIRE(i == *pool.find_component_data({i}));
			}
		}
		SECTION("does not throw when accessing invalid entities") {
			REQUIRE(nullptr == pool.find_component_data(10));
		}
		SECTION("shrinks when entities are removed") {
			pool.remove(4);
			pool.process_changes();

			REQUIRE(pool.num_entities() == 9);
			REQUIRE(pool.num_components() == 9);
			REQUIRE(pool.has_less_components());
		}
		SECTION("becomes empty after clear") {
			pool.clear();
			REQUIRE(pool.num_entities() == 0);
			REQUIRE(pool.num_components() == 0);
			REQUIRE(pool.has_more_components() == false);
			REQUIRE(pool.has_less_components() == true);
		}
		SECTION("remains valid after internal growth") {
			int const* org_p = pool.find_component_data(0);

			for (int i = 10; i < 32; i++) {
				pool.add({i, i}, std::move(i));
				pool.process_changes();
			}

			for (int i = 0; i < 32; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}

			// memory address has not changed
			REQUIRE(org_p == pool.find_component_data(0));
		}

		// No longer the case
		//SECTION("compacts memory on remove") {
		//	pool.remove_range({1, 8});
		//	pool.process_changes();
		//	int const* i0 = pool.find_component_data(0);
		//	int const* i9 = pool.find_component_data(9);
		//	REQUIRE(std::distance(i0, i9) == 1);
		//}
	}

	SECTION("Transient components") {
		SECTION("are automatically removed in process_changes()") {
			struct tr_test {
				ecs_flags(ecs::flag::transient);
			};
			ecs::detail::component_pool<tr_test> pool;
			pool.add({0, 9}, tr_test{});

			pool.process_changes();
			pool.process_changes();
			REQUIRE(pool.num_components() == 0);
		}
	}

	SECTION("Entities") {
		SECTION("maintains sorting") { // test case is response to a found bug
			struct test {
				ecs_flags(ecs::flag::tag);
			};
			ecs::detail::component_pool<test> pool;
			pool.add({0, 0}, {});
			pool.process_changes();
			pool.add({-2, -2}, {});
			pool.process_changes();

			auto const ev = pool.get_entities();
			REQUIRE(ev.front().first() == -2);
		}

		SECTION("produce correct entity_ranges") {
			ecs::detail::component_pool<int> pool;
			pool.add({0, 9}, {});
			pool.process_changes();

			auto const ev = pool.get_entities();
			REQUIRE(ev.size() == 1);
		}

		SECTION("removal from front does not create new ranges") {
			ecs::detail::component_pool<int> pool;
			pool.add({0, 9}, {});
			pool.process_changes();
			pool.remove_range({0, 3});
			pool.process_changes();

			auto const ev = pool.get_entities();
			REQUIRE(ev.size() == 1);
		}

		SECTION("removal from back does not create new ranges") {
			ecs::detail::component_pool<int> pool;
			pool.add({0, 9}, {});
			pool.process_changes();
			pool.remove_range({7, 9});
			pool.process_changes();

			auto const ev = pool.get_entities();
			REQUIRE(ev.size() == 1);
		}

		SECTION("removal from middle results in 2 ranges") {
			ecs::detail::component_pool<int> pool;
			pool.add({0, 9}, {});
			pool.process_changes();
			pool.remove_range({4, 5});
			pool.process_changes();

			auto const ev = pool.get_entities();
			REQUIRE(ev.size() == 2);
		}

		SECTION("two removals from middle results in 3 ranges") {
			ecs::detail::component_pool<int> pool;
			pool.add({0, 9}, {});
			pool.process_changes();
			pool.remove_range({2, 3});
			pool.remove_range({7, 8});
			pool.process_changes();

			auto const ev = pool.get_entities();
			REQUIRE(ev.size() == 3);
		}
	}

	/* not currently implemented
	SECTION("Allocators") {
		SECTION("setting a memory_resource works") {
			constexpr ptrdiff_t buffer_size = 64;
			std::byte buffer[buffer_size]{};
			std::pmr::monotonic_buffer_resource resource(buffer, buffer_size);

			struct test {
				int x;
			};

			ecs::detail::component_pool<test> pool;

			// no resource set
			pool.add({0, 3}, {42});
			pool.process_changes();

			// set the memory resource
			// moves existing data into the new resource
			pool.set_memory_resource(&resource);

			// Verify the data survived
			test const* ent_0_data = pool.find_component_data(0);
			REQUIRE(ent_0_data->x == 42);

			// Verify the data is in the monotonic resource buffer
			std::byte const* t = reinterpret_cast<std::byte const*>(ent_0_data);

			ptrdiff_t const diff = (t - &buffer[0]);

			// * Not implemented yet
			//REQUIRE((diff >= 0 && diff < buffer_size));
		}

		SECTION("memory_resource is propagated to component members where supported") {
			constexpr ptrdiff_t buffer_size = 1024;
			std::byte buffer[buffer_size]{};
			std::pmr::monotonic_buffer_resource resource(buffer, buffer_size);

			struct test {
				int x;
				std::pmr::string s;

				test(int _x, std::pmr::string _s) : x{_x}, s{_s} {}

				// implement pmr support
				ECS_USE_PMR(test);
				explicit test(allocator_type alloc) noexcept : x{}, s{alloc} {}
				test(test const& t, allocator_type alloc) : x{t.x}, s{t.s, alloc} {}
				test(test&& t, allocator_type alloc) : x{t.x}, s{std::move(t.s), alloc} {}
			};

			ecs::detail::component_pool<test> pool;

			// No resource set, data goes on the heap
			pool.add({0, 3}, {42, "hello you saucy minx"});
			pool.process_changes();

			// Set the memory resource.
			// Moves existing data into the new resource
			pool.set_memory_resource(&resource);

			test const* ptr_test = pool.find_component_data(0);
			char const* ptr_string_data = ptr_test->s.data();

			std::byte const* ptr = reinterpret_cast<std::byte const*>(ptr_string_data);
			ptrdiff_t diff = (ptr - &buffer[0]);

			// Verify the data was moved into the monotonic resource
			// * Not implemented yet
			//REQUIRE((diff >= 0 && diff < buffer_size));

			// Add another component. Should go into the monotonic resource
			pool.add({4, 4}, {11, "xxxxxxxxxxxxxxxxxxxxxxxxxxxx"});
			pool.process_changes();

			ptr_test = pool.find_component_data(4);
			ptr_string_data = ptr_test->s.data();

			ptr = reinterpret_cast<std::byte const*>(ptr_string_data);
			diff = (ptr - &buffer[0]);

			// Verify the data was placed in the monotonic resource
			// * Not implemented yet
			// REQUIRE((diff >= 0 && diff < buffer_size));
		}

		SECTION("reseting memory_resource works") {
			ecs::runtime ecs;
			// get the unmodified memory resource
			auto const res = ecs.get_memory_resource<int>();

			// change the resource
			std::pmr::monotonic_buffer_resource dummy;
			ecs.set_memory_resource<int>(&dummy);
			auto const changed_res = ecs.get_memory_resource<int>();
			// * Not implemented yet
			// REQUIRE(&dummy == changed_res);

			// reset
			ecs.reset_memory_resource<int>();

			// verify resource is reverted
			auto const reset_res = ecs.get_memory_resource<int>();
			// * Not implemented yet
			// REQUIRE(res == reset_res);
		}
	}
	*/
}

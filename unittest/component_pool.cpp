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
	ctr_counter(ctr_counter const& /*other*/) noexcept {
		copy_count++;
		ctr_count++;
	}
	ctr_counter(ctr_counter&& /*other*/) noexcept {
		move_count++;
		ctr_count++;
	}
	~ctr_counter() noexcept {
		dtr_count++;
	}

	ctr_counter& operator=(ctr_counter&&) = default;
	ctr_counter& operator=(ctr_counter const&) = default;
};


// A bunch of tests to ensure that the component_pool behaves as expected
TEST_CASE("Component pool specification", "[component]") {
	SECTION("A new component pool is empty") {
		auto const test = [] {
			ecs::detail::component_pool<int> pool;
			return
				pool.num_entities() == 0 &&
				pool.num_components() == 0 &&
				pool.has_component_count_changed() == false;
		};
		static_assert(test());
		REQUIRE(test());
	}

	SECTION("An empty pool") {
		// It won't throw, it will terminate
		/*SECTION("does not throw on bad remove") {
			pool.remove(0);
			pool.process_changes();
			SUCCEED();
		}*/
		SECTION("does not throw on bad component access") {
			auto const test = [] {
				ecs::detail::component_pool<int> pool;
				return nullptr == pool.find_component_data(0);
			};
			static_assert(test());
			REQUIRE(test());
		}
		SECTION("grows when data is added to it") {
			auto const test = [] {
				ecs::detail::component_pool<int> pool;
				pool.add({0, 4}, 0);
				pool.process_changes();

				return (pool.num_entities() == 5) &&
					(pool.num_components() == 5) &&
					(pool.has_more_components());
			};
			static_assert(test());
			REQUIRE(test());
		}
	}

	SECTION("Adding components") {
		SECTION("does not perform unneccesary copies of components") {
			auto const test = [] {
				ecs::detail::component_pool<ctr_counter> pool;
				pool.add({0, 2}, ctr_counter{});
				pool.process_changes();
				pool.remove_range({0, 2});
				pool.process_changes();

				return (ctr_counter::copy_count == 3) && (ctr_counter::ctr_count == ctr_counter::dtr_count);
			};
			//static_assert(test()); // uses static member vars
			REQUIRE(test());
		}
		SECTION("with a span is valid") {
			auto const test = [] {
				std::vector<int> ints(10);
				std::iota(ints.begin(), ints.end(), 0);

				ecs::detail::component_pool<int> pool;
				pool.add_span({0, 9}, ints);
				pool.process_changes();

				for (int i = 0; i <= 9; i++) {
					if (i != *pool.find_component_data(i))
						return false;
				}

				return true;
			};
			//static_assert(test());	// std::function is not constexpr
			REQUIRE(test());
		}
		SECTION("with negative entity ids is fine") {
			auto const test = [] {
				ecs::detail::component_pool<int> pool;
				pool.add({-999, -950}, 0);
				pool.process_changes();

				return (50 == pool.num_components()) &&
					(50 == pool.num_entities());
			};
			static_assert(test());
			REQUIRE(test());
		}
		SECTION("keeps them sorted by entity id") {
			auto const test = [] {
				ecs::detail::component_pool<int> pool;
				pool.add({4, 4}, 4);
				pool.add({1, 1}, 1);
				pool.add({2, 2}, 2);
				pool.process_changes();
				if (pool.find_component_data(1) > pool.find_component_data(2))
					return false;
				if (pool.find_component_data(2) > pool.find_component_data(4))
					return false;

				pool.add({9, 9}, 9);
				pool.add({3, 3}, 3);
				pool.add({7, 7}, 7);
				pool.process_changes();

				return
					(pool.find_component_data(1) < pool.find_component_data(2)) &&
					(pool.find_component_data(2) < pool.find_component_data(3)) &&
					(pool.find_component_data(3) < pool.find_component_data(4)) &&
					(pool.find_component_data(4) < pool.find_component_data(7)) &&
					(pool.find_component_data(7) < pool.find_component_data(9));
			};
			static_assert(test());
			REQUIRE(test());
		}
	}

	SECTION("Removing components") {
		SECTION("from the back does not invalidate other components") {
			auto const test = [] {
				std::vector<int> ints(11);
				std::iota(ints.begin(), ints.end(), 0);

				ecs::detail::component_pool<int> pool;
				pool.add_span({0, 10}, ints);
				pool.process_changes();

				pool.remove_range({9, 10});
				pool.process_changes();

				if (pool.num_components() != 9)
					return false;

				for (int i = 0; i <= 8; i++) {
					if (i != *pool.find_component_data(i))
						return false;
				}

				return true;
			};
			static_assert(test());
			REQUIRE(test());
		}
		SECTION("from the front does not invalidate other components") {
			auto const test = [] {
				std::vector<int> ints(11);
				std::iota(ints.begin(), ints.end(), 0);

				ecs::detail::component_pool<int> pool;
				pool.add_span({0, 10}, ints);
				pool.process_changes();

				pool.remove_range({0, 1});
				pool.process_changes();

				if (pool.num_components() != 9)
					return false;

				for (int i = 2; i <= 10; i++) {
					if (i != *pool.find_component_data(i))
						return false;
				}

				return true;
			};
			static_assert(test());
			REQUIRE(test());
		}
		SECTION("from the middle does not invalidate other components") {
			auto const test = [] {
				std::vector<int> ints(11);
				std::iota(ints.begin(), ints.end(), 0);

				ecs::detail::component_pool<int> pool;
				pool.add_span({0, 10}, ints);
				pool.process_changes();
				
				pool.remove_range({4, 5});
				pool.process_changes();

				if (pool.num_components() != 9)
					return false;

				for (int i = 0; i <= 3; i++) {
					if (i != *pool.find_component_data(i))
						return false;
				}
				for (int i = 6; i <= 10; i++) {
					if (i != *pool.find_component_data(i))
						return false;
				}

				return true;
			};
			static_assert(test());
			REQUIRE(test());
		}

		SECTION("piecewise does not invalidate other components") {
			auto const test = [] {
				std::vector<int> ints(11);
				std::iota(ints.begin(), ints.end(), 0);

				ecs::detail::component_pool<int> pool;
				pool.add_span({0, 10}, ints);
				pool.process_changes();
				
				pool.remove_range({10, 10});
				pool.remove_range({9, 9});
				pool.process_changes();

				if (pool.num_components() != 9)
					return false;

				for (int i = 0; i <= 8; i++) {
					if (i != *pool.find_component_data(i))
						return false;
				}

				return true;
			};
			static_assert(test());
			REQUIRE(test());
		}
	}

	SECTION("A non empty pool") {
		auto const test = [] {
			std::vector<int> ints(10);
			std::iota(ints.begin(), ints.end(), 0);

			ecs::detail::component_pool<int> pool;
			pool.add_span({0, 9}, ints);
			pool.process_changes();

			// "has the correct entities"
			if (10 != pool.num_entities())
				return false;
			if (!pool.has_entity({0, 9}))
				return false;

			// "has the correct components"
			if (10 != pool.num_components())
				return false;
			for (int i = 0; i <= 9; i++) {
				if (i != *pool.find_component_data({i}))
					return false;
			}

			// "does not throw when accessing invalid entities"
			if (nullptr != pool.find_component_data(10))
				return false;

			// "shrinks when entities are removed"
			pool.remove(4);
			pool.process_changes();

			if (9 != pool.num_entities())
				return false;
			if (9 != pool.num_components())
				return false;
			if (!pool.has_less_components())
				return false;

			// "becomes empty after clear"
			pool.clear();
			if (0 != pool.num_entities())
				return false;
			if (0 != pool.num_components())
				return false;
			if (pool.has_more_components())
				return false;
			if (!pool.has_less_components())
				return false;

			// "remains valid after internal growth"
			int const* org_p = pool.find_component_data(0);

			for (int i = 10; i < 32; i++) {
				pool.add({i, i}, i);
				pool.process_changes();
			}

			for (int i = 10; i < 32; i++) {
				if (i != *pool.find_component_data(i))
					return false;
			}

			// memory address has changed
			if (org_p != pool.find_component_data(0))
				return false;

			// "compacts memory on remove"
			pool.remove_range({11, 18});
			pool.process_changes();

			int const* i0 = pool.find_component_data(10);
			int const* i9 = pool.find_component_data(19);
			if (1 != std::distance(i0, i9))
				return false;

			return true;
		};
		static_assert(test());
		REQUIRE(test());
	}

	SECTION("Transient components") {
		SECTION("are automatically removed in process_changes()") {
			auto const test = [] {
				struct tr_test {
					ecs_flags(ecs::flag::transient);
				};
				ecs::detail::component_pool<tr_test> pool;
				pool.add({0, 9}, tr_test{});

				pool.process_changes();
				pool.process_changes();
				if (0 != pool.num_components())
					return false;

				return true;
			};
			static_assert(test());
			REQUIRE(test());
		}
	}

	SECTION("Tagged components") {
		SECTION("maintains sorting of entities") { // test case is response to a found bug
			auto const test = [] {
				struct some_tag {
					ecs_flags(ecs::flag::tag);
				};
				ecs::detail::component_pool<some_tag> pool;
				pool.add({0, 0}, {});
				pool.process_changes();
				pool.add({-2, -2}, {});
				pool.process_changes();

				auto const ev = pool.get_entities();
				return (ev.front().first() == -2);
			};
			static_assert(test());
			REQUIRE(test());
		}
	}

	/*SECTION("Allocators"){
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

			REQUIRE((diff >= 0 && diff < buffer_size));
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
			REQUIRE((diff >= 0 && diff < buffer_size));

			// Add another component. Should go into the monotonic resource
			pool.add({4, 4}, {11, "xxxxxxxxxxxxxxxxxxxxxxxxxxxx"});
			pool.process_changes();

			ptr_test = pool.find_component_data(4);
			ptr_string_data = ptr_test->s.data();

			ptr = reinterpret_cast<std::byte const*>(ptr_string_data);
			diff = (ptr - &buffer[0]);

			// Verify the data was placed in the monotonic resource
			REQUIRE((diff >= 0 && diff < buffer_size));
		}

		SECTION("reseting memory_resource works") {
			ecs::runtime ecs;
			// get the unmodified memory resource
			auto const res = ecs.get_memory_resource<int>();

			// change the resource
			std::pmr::monotonic_buffer_resource dummy;
			ecs.set_memory_resource<int>(&dummy);
			auto const changed_res = ecs.get_memory_resource<int>();
			REQUIRE(&dummy == changed_res);

			// reset
			ecs.reset_memory_resource<int>();

			// verify resource is reverted
			auto const reset_res = ecs.get_memory_resource<int>();
			REQUIRE(res == reset_res);
		}
	}*/
}

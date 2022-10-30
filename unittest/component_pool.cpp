#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>

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
	auto x = sizeof(ecs::detail::component_pool<int>);
	(void)x;
	auto y = sizeof(tls::collect<std::vector<int>>);
	(void)y;
	auto z = alignof(void*);
	(void)z;

	SECTION("A new component pool is empty") {
		ecs::detail::component_pool<int> pool;
		REQUIRE(pool.num_entities() == 0);
		REQUIRE(pool.num_components() == 0);
		REQUIRE(pool.has_component_count_changed() == false);
	}

	SECTION("An empty pool") {
		SECTION("does not throw on bad component access") {
			ecs::detail::component_pool<int> pool;
			REQUIRE(nullptr == pool.find_component_data(0));
		}
		SECTION("grows when data is added to it") {
			ecs::detail::component_pool<int> pool;
			pool.add({0, 4}, 0);
			pool.process_changes();

			REQUIRE(pool.num_entities() == 5);
			REQUIRE(pool.num_components() == 5);
			REQUIRE(pool.has_more_components());
		}
	}

	SECTION("Adding components") {
		SECTION("does not perform unneccesary copies of components") {
			ecs::detail::component_pool<ctr_counter> pool;
			pool.add({0, 2}, ctr_counter{});
			pool.process_changes();
			pool.remove({0, 2});
			pool.process_changes();

			static constexpr std::size_t expected_copy_count = 3;
			REQUIRE(ctr_counter::copy_count == expected_copy_count);
			REQUIRE(ctr_counter::ctr_count == ctr_counter::dtr_count);
		}
		SECTION("with a span is valid") {
			std::vector<int> ints(10);
			std::iota(ints.begin(), ints.end(), 0);

			ecs::detail::component_pool<int> pool;
			pool.add_span({0, 9}, ints);
			pool.process_changes();

			REQUIRE(10 == pool.num_components());

			for (int i = 0; i <= 9; i++) {
				REQUIRE (i == *pool.find_component_data(i));
			}
		}
		SECTION("with a generator is valid") {
			ecs::detail::component_pool<int> pool;
			pool.add_generator({0, 9}, [](auto i) {
				return static_cast<int>(i);
			});
			pool.process_changes();

			REQUIRE(10 == pool.num_components());

			for (int i = 0; i <= 9; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}
		}
		SECTION("with negative entity ids is fine") {
			ecs::detail::component_pool<int> pool;
			pool.add({-999, -950}, 0);
			pool.process_changes();

			REQUIRE(50 == pool.num_components());
			REQUIRE(50 == pool.num_entities());
		}
	}

	SECTION("Removing components") {
		SECTION("from the back does not invalidate other components") {
			std::vector<int> ints(11);
			std::iota(ints.begin(), ints.end(), 0);

			ecs::detail::component_pool<int> pool;
			pool.add_span({0, 10}, ints);
			pool.process_changes();

			pool.remove({9, 10});
			pool.process_changes();

			REQUIRE(pool.num_components() == 9);
			for (int i = 0; i <= 8; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}
		}
		SECTION("from the front does not invalidate other components") {
			std::vector<int> ints(11);
			std::iota(ints.begin(), ints.end(), 0);

			ecs::detail::component_pool<int> pool;
			pool.add_span({0, 10}, ints);
			pool.process_changes();

			pool.remove({0, 1});
			pool.process_changes();

			REQUIRE(pool.num_components() == 9);
			for (int i = 2; i <= 10; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}
		}
		SECTION("from the middle does not invalidate other components") {
			std::vector<int> ints(11);
			std::iota(ints.begin(), ints.end(), 0);

			ecs::detail::component_pool<int> pool;
			pool.add_span({0, 10}, ints);
			pool.process_changes();

			pool.remove({4, 5});
			pool.process_changes();

			REQUIRE(pool.num_components() == 9);
			for (int i = 0; i <= 3; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}
			for (int i = 6; i <= 10; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}
		}
		SECTION("piecewise does not invalidate other components") {
			std::vector<int> ints(11);
			std::iota(ints.begin(), ints.end(), 0);

			ecs::detail::component_pool<int> pool;
			pool.add_span({0, 10}, ints);
			pool.process_changes();

			pool.remove({10, 10});
			pool.remove({9, 9});
			pool.process_changes();

			REQUIRE(pool.num_components() == 9);
			for (int i = 0; i <= 8; i++) {
				REQUIRE(i == *pool.find_component_data(i));
			}
		}
		SECTION("that span multiple chunks") {
			ecs::detail::component_pool<int> pool;
			pool.add({0, 5}, int{});
			pool.process_changes();
			pool.add({6, 10}, int{});
			pool.process_changes();

			pool.remove({0, 10});
			pool.process_changes();

			REQUIRE(pool.num_components() == 0);
		}
	}

	SECTION("A non empty pool") {
		std::vector<int> ints(10);
		std::iota(ints.begin(), ints.end(), 0);

		ecs::detail::component_pool<int> pool;
		pool.add_span({0, 9}, ints);
		pool.process_changes();

		// "has the correct entities"
		REQUIRE(10 == pool.num_entities());
		REQUIRE(pool.has_entity({0, 9}));

		// "has the correct components"
		REQUIRE(10 == pool.num_components());
		for (int i = 0; i <= 9; i++) {
			REQUIRE(i == *pool.find_component_data({i}));
		}

		// "does not throw when accessing invalid entities"
		REQUIRE(nullptr == pool.find_component_data(10));

		// "shrinks when entities are removed"
		pool.remove({4});
		pool.process_changes();

		REQUIRE(9 == pool.num_entities());
		REQUIRE(9 == pool.num_components());
		REQUIRE(pool.has_less_components());

		// "becomes empty after clear"
		pool.clear();
		REQUIRE(0 == pool.num_entities());
		REQUIRE(0 == pool.num_components());
		REQUIRE(!pool.has_more_components());
		REQUIRE(pool.has_less_components());

		// "remains valid after internal growth"
		int const* org_p = pool.find_component_data(0);

		for (int i = 10; i < 32; i++) {
			pool.add({i, i}, i);
			pool.process_changes();
		}

		for (int i = 10; i < 32; i++) {
			REQUIRE(i == *pool.find_component_data(i));
		}

		// memory address has not changed
		REQUIRE(org_p == pool.find_component_data(0));
	}

	SECTION("Transient components") {
		SECTION("are automatically removed in process_changes()") {
			struct tr_test {
				ecs_flags(ecs::flag::transient);
			};
			ecs::detail::component_pool<tr_test> pool;
			pool.add({0, 9}, tr_test{});

			pool.process_changes(); // added
			pool.process_changes(); // automatically removed
			REQUIRE(0 == pool.num_components());
		}
	}

	SECTION("Tagged components") {
		SECTION("maintains sorting of entities") { // test case is response to a found bug
			struct some_tag {
				ecs_flags(ecs::flag::tag);
			};
			ecs::detail::component_pool<some_tag> pool;
			pool.add({0, 0}, {});
			pool.process_changes();
			pool.add({-2, -2}, {});
			pool.process_changes();

			auto const ev = pool.get_entities();
			REQUIRE(ev.current()->first() == -2);
		}
	}

	SECTION("Global components") {
		SECTION("are always available") {
			struct some_global {
				ecs_flags(ecs::flag::global);
				int v = 0;
			};
			ecs::detail::component_pool<some_global> pool;

			// if the component is not available, this will crash/fail
			pool.get_shared_component().v += 1;
		}
	}

	SECTION("chunked memory") {
		SECTION("a range of components is contiguous in memory") {
			ecs::detail::component_pool<int> pool;
			pool.add({1, 3}, 0);
			pool.process_changes();
			CHECK(1 == pool.num_chunks());

			auto const ptr1 = pool.find_component_data(1);
			auto const ptr3 = pool.find_component_data(3);
			REQUIRE(ptrdiff_t{2} == std::distance(ptr1, ptr3));
		}

		SECTION("insertion order forward is correct") {
			ecs::detail::component_pool<int> pool;
			pool.add({1, 1}, 0);
			pool.add({3, 3}, 0);
			pool.add({5, 5}, 0);
			pool.process_changes();

			// There should be 3 chunks
			CHECK(3 == pool.num_chunks());

			// They should be properly ordered
			auto chunk = pool.get_head_chunk();
			REQUIRE(chunk->range < std::next(chunk)->range);

			// They should be seperate
			REQUIRE(chunk->data != std::next(chunk)->data);
		}

		SECTION("insertion order backward is correct") {
			ecs::detail::component_pool<int> pool;
			pool.add({5, 5}, 0);
			pool.add({3, 3}, 0);
			pool.add({1, 1}, 0);
			pool.process_changes();

			// There should be 3 chunks
			CHECK(3 == pool.num_chunks());

			// They should be properly ordered
			auto chunk = pool.get_head_chunk();
			REQUIRE(chunk->range < std::next(chunk)->range);

			// They should be seperate
			REQUIRE(chunk->data != std::next(chunk)->data);
		}

		SECTION("splitting a range preserves locations in memory") {
			ecs::detail::component_pool<int> pool;
			pool.add({1, 3}, 0);
			pool.process_changes();
			pool.remove(2);
			pool.process_changes();
			CHECK(2 == pool.num_chunks());

			auto const ptr1 = pool.find_component_data(1);
			auto const ptr2 = pool.find_component_data(2);
			auto const ptr3 = pool.find_component_data(3);
			REQUIRE(ptrdiff_t{2} == std::distance(ptr1, ptr3));
			REQUIRE(nullptr == ptr2);
		}

		SECTION("filling several gaps in a range reduces chunk count") {
			ecs::detail::component_pool<int> pool;
			// Add a range from 1 to 5, will result in 1 chunk
			pool.add({1, 5}, 0);
			pool.process_changes();

			// Poke 2 holes in the range, will result in 3 chunks
			pool.remove(2);
			pool.remove(4);
			pool.process_changes();
			CHECK(3 == pool.num_chunks());

			// Fill the 2 holes, will result in 1 chunk again
			pool.add({4, 4}, 1);
			pool.add({2, 2}, 1);
			pool.process_changes();
			CHECK(1 == pool.num_chunks());

			// Verify memory addresses of the components
			auto const ptr1 = pool.find_component_data(1);
			auto const ptr2 = pool.find_component_data(2);
			auto const ptr3 = pool.find_component_data(3);
			REQUIRE(ptrdiff_t{2} == std::distance(ptr1, ptr3));
			REQUIRE(ptr2 > ptr1);
			REQUIRE(ptr2 < ptr3);
		}

		SECTION("filling gaps in reverse moves ownership to first chunk") {
			ecs::detail::component_pool<int> pool;
			pool.add({1, 5}, 5);
			pool.process_changes();

			pool.remove({1, 4});
			pool.process_changes();

			// only '5' remains, which is now owner of the data
			REQUIRE(1 == pool.num_chunks());
			auto chunk = pool.get_head_chunk();
			REQUIRE(chunk->range.equals({1, 5}));
			REQUIRE(chunk->active.equals({5, 5}));
			REQUIRE(chunk->get_owns_data());

			// 3 is now first entity, so it is now owner
			pool.add({3, 3}, 3);
			pool.process_changes();
			REQUIRE(2 == pool.num_chunks());
			chunk = pool.get_head_chunk();
			REQUIRE(chunk->active.equals({3, 3}));
			REQUIRE(chunk->get_owns_data());
			//REQUIRE(nullptr != chunk->next);
			REQUIRE(false == std::next(chunk)->get_owns_data());

			// Fill in rest
			pool.add({1, 1}, 1);
			pool.add({4, 4}, 4);
			pool.add({2, 2}, 2);
			pool.process_changes();

			CHECK(1 == pool.num_chunks());
			chunk = pool.get_head_chunk();
			REQUIRE(chunk->active.equals({1, 5}));
			REQUIRE(chunk->get_owns_data());
			//REQUIRE(nullptr == chunk->next);

			// Verify the component data
			REQUIRE(1 == *pool.find_component_data(1));
			REQUIRE(2 == *pool.find_component_data(2));
			REQUIRE(3 == *pool.find_component_data(3));
			REQUIRE(4 == *pool.find_component_data(4));
			REQUIRE(5 == *pool.find_component_data(5));
		}

		SECTION("filling gaps in unrelated ranges") {
			ecs::detail::component_pool<int> pool;
			pool.add({1, 1}, 0);
			pool.add({3, 3}, 0);
			pool.add({5, 5}, 0);
			pool.process_changes();

			// There should be 3 chunks
			CHECK(3 == pool.num_chunks());

			// They should be properly ordered
			auto chunk = pool.get_head_chunk();
			REQUIRE(chunk->range < std::next(chunk)->range);

			// They should be seperate
			REQUIRE(chunk->data != std::next(chunk)->data);
		}
	}
}

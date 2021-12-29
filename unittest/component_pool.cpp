#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>
#include <memory_resource>
#include <string>


#if __cpp_lib_constexpr_vector && __cpp_constexpr_dynamic_alloc
#define CONSTEXPR_UNITTEST(t) static_assert((t))
#else
#define CONSTEXPR_UNITTEST(t) ((void)0)
#endif


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
		auto const test = [] {
			ecs::detail::component_pool<int> pool;
			return pool.num_entities() == 0 && pool.num_components() == 0 && pool.has_component_count_changed() == false;
		};
		CONSTEXPR_UNITTEST(test());
		REQUIRE(test());
	}

	SECTION("An empty pool") {
		SECTION("does not throw on bad component access") {
			auto const test = [] {
				ecs::detail::component_pool<int> pool;
				return nullptr == pool.find_component_data(0);
			};
			CONSTEXPR_UNITTEST(test());
			REQUIRE(test());
		}
		SECTION("grows when data is added to it") {
			auto const test = [] {
				ecs::detail::component_pool<int> pool;
				pool.add({0, 4}, 0);
				pool.process_changes();

				return (pool.num_entities() == 5) && (pool.num_components() == 5) && (pool.has_more_components());
			};
			CONSTEXPR_UNITTEST(test());
			REQUIRE(test());
		}
	}

	SECTION("Adding components") {
		SECTION("does not perform unneccesary copies of components") {
			auto const test = [] {
				ecs::detail::component_pool<ctr_counter> pool;
				pool.add({0, 2}, ctr_counter{});
				pool.process_changes();
				pool.remove({0, 2});
				pool.process_changes();

				return (ctr_counter::copy_count == 3) && (ctr_counter::ctr_count == ctr_counter::dtr_count);
			};
			// CONSTEXPR_UNITTEST(test()); // uses static member vars
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
			CONSTEXPR_UNITTEST(test());
			REQUIRE(test());
		}
		SECTION("with negative entity ids is fine") {
			auto const test = [] {
				ecs::detail::component_pool<int> pool;
				pool.add({-999, -950}, 0);
				pool.process_changes();

				return (50 == pool.num_components()) && (50 == pool.num_entities());
			};
			CONSTEXPR_UNITTEST(test());
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

				pool.remove({9, 10});
				pool.process_changes();

				if (pool.num_components() != 9)
					return false;

				for (int i = 0; i <= 8; i++) {
					if (i != *pool.find_component_data(i))
						return false;
				}

				return true;
			};
			CONSTEXPR_UNITTEST(test());
			REQUIRE(test());
		}
		SECTION("from the front does not invalidate other components") {
			auto const test = [] {
				std::vector<int> ints(11);
				std::iota(ints.begin(), ints.end(), 0);

				ecs::detail::component_pool<int> pool;
				pool.add_span({0, 10}, ints);
				pool.process_changes();

				pool.remove({0, 1});
				pool.process_changes();

				if (pool.num_components() != 9)
					return false;

				for (int i = 2; i <= 10; i++) {
					if (i != *pool.find_component_data(i))
						return false;
				}

				return true;
			};
			CONSTEXPR_UNITTEST(test());
			REQUIRE(test());
		}
		SECTION("from the middle does not invalidate other components") {
			auto const test = [] {
				std::vector<int> ints(11);
				std::iota(ints.begin(), ints.end(), 0);

				ecs::detail::component_pool<int> pool;
				pool.add_span({0, 10}, ints);
				pool.process_changes();

				pool.remove({4, 5});
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
			CONSTEXPR_UNITTEST(test());
			REQUIRE(test());
		}

		SECTION("piecewise does not invalidate other components") {
			auto const test = [] {
				std::vector<int> ints(11);
				std::iota(ints.begin(), ints.end(), 0);

				ecs::detail::component_pool<int> pool;
				pool.add_span({0, 10}, ints);
				pool.process_changes();

				pool.remove({10, 10});
				pool.remove({9, 9});
				pool.process_changes();

				if (pool.num_components() != 9)
					return false;

				for (int i = 0; i <= 8; i++) {
					if (i != *pool.find_component_data(i))
						return false;
				}

				return true;
			};
			CONSTEXPR_UNITTEST(test());
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

			return true;
		};
		CONSTEXPR_UNITTEST(test());
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
			CONSTEXPR_UNITTEST(test());
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
			CONSTEXPR_UNITTEST(test());
			REQUIRE(test());
		}
	}

	SECTION("Global components") {
		SECTION("are always available") {
			auto const test = [] {
				struct some_global {
					ecs_flags(ecs::flag::global);
				};
				ecs::detail::component_pool<some_global> pool;
				return (&pool.get_shared_component() != nullptr);
			};
			CONSTEXPR_UNITTEST(test());
			REQUIRE(test());
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
			REQUIRE(chunk->range < chunk->next->range);

			// They should be seperate
			REQUIRE(chunk->data != chunk->next->data);
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
			REQUIRE(chunk->range < chunk->next->range);

			// They should be seperate
			REQUIRE(chunk->data != chunk->next->data);
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
			REQUIRE(chunk->owns_data);

			// 3 is now first entity, so it is now owner
			pool.add({3, 3}, 3);
			pool.process_changes();
			REQUIRE(2 == pool.num_chunks());
			chunk = pool.get_head_chunk();
			REQUIRE(chunk->active.equals({3, 3}));
			REQUIRE(chunk->owns_data);
			REQUIRE(nullptr != chunk->next);
			REQUIRE(false == chunk->next->owns_data);

			// Fill in rest
			pool.add({1, 1}, 1);
			pool.add({4, 4}, 4);
			pool.add({2, 2}, 2);
			pool.process_changes();

			CHECK(1 == pool.num_chunks());
			chunk = pool.get_head_chunk();
			REQUIRE(chunk->active.equals({1, 5}));
			REQUIRE(chunk->owns_data);
			REQUIRE(nullptr == chunk->next);

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
			REQUIRE(chunk->range < chunk->next->range);

			// They should be seperate
			REQUIRE(chunk->data != chunk->next->data);
		}
	}
}

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
    ~ctr_counter() { dtr_count++; }

    ctr_counter& operator=(ctr_counter&&) = default;
    ctr_counter& operator=(ctr_counter const&) = default;
};

// A bunch of tests to ensure that the component_pool behaves as expected
TEST_CASE("Component pool specification", "[component]") {
    SECTION("A new component pool") {
        SECTION("is empty") {
            ecs::detail::component_pool<int> pool;
            CHECK(pool.num_entities() == 0);
            CHECK(pool.num_components() == 0);
            CHECK(pool.is_data_modified() == false);
        }
    }

    SECTION("An empty pool") {
        ecs::detail::component_pool<int> pool;

        SECTION("does not throw on bad remove") {
            pool.remove(0);
            pool.process_changes();
            SUCCEED();
        }
        SECTION("does not throw on bad component access") { CHECK(nullptr == pool.find_component_data(0)); }
        SECTION("grows when data is added to it") {
            pool.add({0, 4}, 0);
            pool.process_changes();

            CHECK(pool.num_entities() == 5);
            CHECK(pool.num_components() == 5);
            CHECK(pool.is_data_added());
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

            for (int i = 0; i <= 9; i++) { CHECK(i == *pool.find_component_data(i)); }
        }
        SECTION("with negative entity ids is fine") {
            ecs::detail::component_pool<int> pool;
            pool.add({-999, -950}, 0);
            pool.process_changes();

            CHECK(50 == pool.num_components());
            CHECK(50 == pool.num_entities());
        }
        SECTION("keeps them sorted by entity id") {
            ecs::detail::component_pool<int> pool;
            pool.add(4, 4);
            pool.add(1, 1);
            pool.add(2, 2);
            pool.process_changes();
            CHECK(pool.find_component_data(1) < pool.find_component_data(2));
            CHECK(pool.find_component_data(2) < pool.find_component_data(4));

            pool.add(9, 9);
            pool.add(3, 3);
            pool.add(7, 7);
            pool.process_changes();

            CHECK(pool.find_component_data(1) < pool.find_component_data(2));
            CHECK(pool.find_component_data(2) < pool.find_component_data(3));
            CHECK(pool.find_component_data(3) < pool.find_component_data(4));
            CHECK(pool.find_component_data(4) < pool.find_component_data(7));
            CHECK(pool.find_component_data(7) < pool.find_component_data(9));
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
            for (int i = 0; i <= 8; i++) { REQUIRE(i == *pool.find_component_data(i)); }
        }
        SECTION("from the front does not invalidate other components") {
            pool.remove_range({0, 1});
            pool.process_changes();

            REQUIRE(pool.num_components() == 9);
            for (int i = 2; i <= 10; i++) { REQUIRE(i == *pool.find_component_data(i)); }
        }
        SECTION("from the middle does not invalidate other components") {
            pool.remove_range({4, 5});
            pool.process_changes();

            REQUIRE(pool.num_components() == 9);
            for (int i = 0; i <= 3; i++) { REQUIRE(i == *pool.find_component_data(i)); }
            for (int i = 6; i <= 10; i++) { REQUIRE(i == *pool.find_component_data(i)); }
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
            for (int i = 0; i <= 9; i++) { REQUIRE(i == *pool.find_component_data({i})); }
        }
        SECTION("does not throw when accessing invalid entities") { REQUIRE(nullptr == pool.find_component_data(10)); }
        SECTION("shrinks when entities are removed") {
            pool.remove(4);
            pool.process_changes();

            REQUIRE(pool.num_entities() == 9);
            REQUIRE(pool.num_components() == 9);
            REQUIRE(pool.is_data_removed());
        }
        SECTION("becomes empty after clear") {
            pool.clear();
            REQUIRE(pool.num_entities() == 0);
            REQUIRE(pool.num_components() == 0);
            REQUIRE(pool.is_data_added() == false);
            REQUIRE(pool.is_data_removed() == true);
        }
        SECTION("remains valid after internal growth") {
            int const* org_p = pool.find_component_data(0);

            for (int i = 10; i < 32; i++) {
                pool.add(i, std::move(i));
                pool.process_changes();
            }

            for (int i = 0; i < 32; i++) { REQUIRE(i == *pool.find_component_data(i)); }

            // memory address has changed
            REQUIRE(org_p != pool.find_component_data(0));
        }
        SECTION("compacts memory on remove") {
            pool.remove_range({1, 8});
            pool.process_changes();

            int const* i0 = pool.find_component_data(0);
            int const* i9 = pool.find_component_data(9);
            REQUIRE(std::distance(i0, i9) == 1);
        }
    }

    SECTION("Transient components") {
        SECTION("are automatically removed in process_changes()") {
            struct tr_test {
                ecs_flags(ecs::transient);
            };
            ecs::detail::component_pool<tr_test> pool;
            pool.add({0, 9}, tr_test{});

            pool.process_changes();
            CHECK(pool.num_components() == 10);

            pool.process_changes();
            REQUIRE(pool.num_components() == 0);
        }
    }

    SECTION("Shared components") {
        // TODO
        SECTION("maintains sorting of entities") { // test case is response to a found bug
            struct test {
                ecs_flags(ecs::share);
            };
            ecs::detail::component_pool<test> pool;
            pool.add(0, {});
            pool.process_changes();
            pool.add(-2, {});
            pool.process_changes();

            auto const ev = pool.get_entities();
            REQUIRE(ev.front().first() == -2);
        }
    }

    SECTION("Tagged components") {
        // TODO
    }
}

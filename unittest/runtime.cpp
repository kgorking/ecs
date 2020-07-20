#include "catch.hpp"
#include <ecs/ecs.h>


struct runtime_ctr_counter {
    inline static size_t def_ctr_count = 0;
    inline static size_t ctr_count = 0;
    inline static size_t copy_count = 0;
    inline static size_t move_count = 0;
    inline static size_t dtr_count = 0;

    runtime_ctr_counter() noexcept {
        def_ctr_count++;
        ctr_count++;
    }
    runtime_ctr_counter(runtime_ctr_counter const& /*other*/) {
        copy_count++;
        ctr_count++;
    }
    runtime_ctr_counter(runtime_ctr_counter&& /*other*/) noexcept {
        move_count++;
        ctr_count++;
    }
    ~runtime_ctr_counter() { dtr_count++; }

    runtime_ctr_counter& operator=(runtime_ctr_counter&&) = default;
    runtime_ctr_counter& operator=(runtime_ctr_counter const&) = default;
};

TEST_CASE("The runtime interface") {
    SECTION("Does perfect forwarding correctly") {
        ecs::detail::_context.reset();
        ecs::add_component({0, 9}, runtime_ctr_counter{});
        ecs::commit_changes();

        CHECK(runtime_ctr_counter::def_ctr_count == 1);
        CHECK(runtime_ctr_counter::move_count == 2);
        CHECK(runtime_ctr_counter::dtr_count == 1 + 2);
        CHECK(runtime_ctr_counter::copy_count == 10);

        ecs::detail::_context.reset();
        CHECK(runtime_ctr_counter::dtr_count == 1 + 2 + 10);
    }

    SECTION("Allocates storage as needed") {
        // Use a local struct to avoid it possibly
        // already existing from another unittest
        struct S {
            size_t c;
        };

        // Add a system-less component to an entity
        ecs::add_component(0, S{0});
        ecs::commit_changes();
        REQUIRE(ecs::get_component_count<S>() == 1);
    }

    SECTION("Supportsd mutable lambdas") {
        struct mut_lambda {
            int i;
        };

        // Add some systems to test
        ecs::make_system([counter = 0](mut_lambda& ml) mutable { ml.i = counter++; });
        ecs::make_system([](ecs::entity_id ent, mut_lambda const& ml) { CHECK(ent == ml.i); });

        // Create 100 entities and add stuff to them
        ecs::add_component({0, 3}, mut_lambda{0});
        ecs::update_systems();
    }

    SECTION("Ranged add") {
        struct range_add {
            int i;
        };

        SECTION("of components works") {
            ecs::add_component({0, 5}, range_add{5});
            ecs::entity_range const ents{6, 9, range_add{5}};
            ecs::commit_changes();

            for (ecs::entity_id i = 0; i <= 9; ++i) {
                auto const& ra = *ecs::get_component<range_add>(i);
                CHECK(ra.i == 5);
            }
        }

        SECTION("of components with initializer works") {
            auto const init = [](auto ent) -> range_add { return {ent * 2}; };

            ecs::add_component({10, 15}, init);
            ecs::entity_range const ents{16, 20, init};

            ecs::commit_changes();

            int i = 10;
            for (auto const& ra : ecs::get_components<range_add>({10, 20})) {
                CHECK(ra.i == i * 2);
                i++;
            }
        }
    }
}

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>


TEST_CASE("Shared components", "[component][shared]") {
    SECTION("Only allocs 1 component") {
        ecs::detail::_context.reset();
        struct test_s {
            ecs_flags(ecs::flag::share);
            int i = 0;
        };

        ecs::detail::_context.reset();

        auto& pst = ecs::get_shared_component<test_s>();
        pst.i = 42;

        ecs::make_system([](test_s const& st) { CHECK(42 == st.i); });

        ecs::add_component({0, 2}, test_s{});
        ecs::commit_changes();

        // Only 1 test_s should exist
        CHECK(1 == ecs::get_component_count<test_s>());

        // Ensure that different entities have the same shared component
        // -- note -- no longer possible
        //ptrdiff_t const diff = ecs::get_component<test_s>(0) - ecs::get_component<test_s>(1);
        //CHECK(diff == 0);

        // Test the content of the entities
        ecs::run_systems();
    }

    SECTION("Multiple shared components do not interact") {
        ecs::detail::_context.reset();
        struct shared1 {
            ecs_flags(ecs::flag::share);
        };
        struct shared2 {
            ecs_flags(ecs::flag::share);
        };

        ecs::detail::component_pool<shared1> ps1;
        ecs::detail::component_pool<shared2> ps2;

        ps1.add({0, 9}, shared1{});
        ps2.add({10, 19}, shared2{});

        CHECK(false == ps2.is_queued_add({0, 9}));
        CHECK(false == ps1.is_queued_add({10, 19}));

        ps1.process_changes();
        ps2.process_changes();

        CHECK(ps1.num_entities() == 10);
        CHECK(ps2.num_entities() == 10);
    }
}

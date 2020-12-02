#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>

TEST_CASE("System specification", "[system]") {
    SECTION("Running a system works") {
        ecs::detail::_context.reset();

        struct local1 {
            int c;
        };
        // Add a system for the local component
        auto& sys = ecs::make_system([](local1& l) { l.c++; });

        // Add the component to an entity
        ecs::add_component(0, local1{0});
        ecs::commit_changes();

        // Run the system 5 times
        for (int i = 0; i < 5; i++) {
            sys.run();
        }

        // Get the component data to verify that the system was run the correct number of times
        auto const l = *ecs::get_component<local1>(0);
        REQUIRE(5U == l.c);
    }

    SECTION("Verify enable/disable functions") {
        ecs::detail::_context.reset();

        struct local2 {};
        auto& sys = ecs::make_system([](local2 const& /*c*/) {});

        REQUIRE(true == sys.is_enabled());
        sys.disable();
        REQUIRE(false == sys.is_enabled());
        sys.enable();
        REQUIRE(true == sys.is_enabled());
        sys.set_enable(false);
        REQUIRE(false == sys.is_enabled());
    }

    SECTION("Disabling systems prevents them from running") {
        ecs::detail::_context.reset();

        struct local3 {
            int c;
        };
        // Add a system for the local component
        auto& sys = ecs::make_system([](local3& l) { l.c++; });

        ecs::add_component(0, local3{0});
        ecs::commit_changes();

        // Run the system and check value
        sys.run();
        REQUIRE(1 == ecs::get_component<local3>(0)->c);

        // Disable system and re-run. Should not change the component
        sys.disable();
        sys.run();
        REQUIRE(1 == ecs::get_component<local3>(0)->c);

        // Enable system and re-run. Should change the component
        sys.enable();
        sys.run();
        REQUIRE(2 == ecs::get_component<local3>(0)->c);
    }

    SECTION("Re-enabling systems forces a rebuild") {
        ecs::detail::_context.reset();

        struct local4 {
            int c;
        };
        // Add a system for the local component
        auto& sys = ecs::make_system([](local4& l) { l.c++; });
        sys.disable();

        ecs::add_component(0, local4{0});
        ecs::commit_changes();
        sys.run();
        REQUIRE(0 == ecs::get_component<local4>(0)->c);

        sys.enable();
        sys.run();
        REQUIRE(1 == ecs::get_component<local4>(0)->c);
    }

    SECTION("Read/write info on systems is correct") {
        ecs::detail::_context.reset();

        auto const& sys1 = ecs::make_system([](int const&, float const&) {});
        CHECK(false == sys1.writes_to_any_components());
        CHECK(false == sys1.writes_to_component(ecs::detail::get_type_hash<int>()));
        CHECK(false == sys1.writes_to_component(ecs::detail::get_type_hash<float>()));

        auto const& sys2 = ecs::make_system([](int&, float const&) {});
        CHECK(true == sys2.writes_to_any_components());
        CHECK(true == sys2.writes_to_component(ecs::detail::get_type_hash<int>()));
        CHECK(false == sys2.writes_to_component(ecs::detail::get_type_hash<float>()));

        auto const& sys3 = ecs::make_system([](int&, float&) {});
        CHECK(true == sys3.writes_to_any_components());
        CHECK(true == sys3.writes_to_component(ecs::detail::get_type_hash<int>()));
        CHECK(true == sys3.writes_to_component(ecs::detail::get_type_hash<float>()));
    }

    SECTION("System with all combinations of types works") {
        ecs::detail::get_context().reset();

        struct vanilla {
            int x;
        };
        struct tagged {
            ecs_flags(ecs::flag::tag);
        };
        struct transient {
            ecs_flags(ecs::flag::transient);
        };
        struct immutable {
            ecs_flags(ecs::flag::immutable);
        };
        struct global {
            ecs_flags(ecs::flag::global);
        };

        auto constexpr vanilla_sort = [](vanilla l, vanilla r) { return l.x < r.x; };

        int last = -100'000'000;
        int run_counter = 0;
        ecs::make_system<ecs::opts::not_parallel>(
            [&](vanilla const& v, tagged, transient const&, immutable const&, global const&, short*) {
                CHECK(last <= v.x);
                last = v.x;

                run_counter++;
            },
            vanilla_sort);

        auto const vanilla_init = [](ecs::entity_id) { return vanilla{rand()}; };
        ecs::add_component({0, 1000}, vanilla_init, tagged{}, transient{}, immutable{});
        ecs::add_component({10, 20}, short{0});

        ecs::update();
        CHECK(run_counter == 1001 - 11);

        last = -100'000'000;
        ecs::update(); // transient component is gone, so system wont run
        CHECK(run_counter == 1001 - 11);
    }
}

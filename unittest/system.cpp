#include "catch.hpp"
#include <ecs/ecs.h>


TEST_CASE("System specification", "[system]") {
    SECTION("Running a system works") {
        struct local1 {
            int c;
        };
        // Add a system for the local component
        auto& sys = ecs::make_system([](local1& l) { l.c++; });

        // Add the component to an entity
        ecs::add_component(0, local1{0});
        ecs::commit_changes();

        // Run the system 5 times
        for (int i = 0; i < 5; i++) { sys.update(); }

        // Get the component data to verify that the system was run the correct number of times
        auto const l = *ecs::get_component<local1>(0);
        REQUIRE(5U == l.c);
    }

    SECTION("Verify enable/disable functions") {
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
        struct local3 {
            int c;
        };
        // Add a system for the local component
        auto& sys = ecs::make_system([](local3& l) { l.c++; });

        ecs::add_component(0, local3{0});
        ecs::commit_changes();

        // Run the system and check value
        sys.update();
        REQUIRE(1 == ecs::get_component<local3>(0)->c);

        // Disable system and re-run. Should not change the component
        sys.disable();
        sys.update();
        REQUIRE(1 == ecs::get_component<local3>(0)->c);

        // Enable system and re-run. Should change the component
        sys.enable();
        sys.update();
        REQUIRE(2 == ecs::get_component<local3>(0)->c);
    }

    SECTION("Re-enabling systems forces a rebuild") {
        struct local4 {
            int c;
        };
        // Add a system for the local component
        auto& sys = ecs::make_system([](local4& l) { l.c++; });
        sys.disable();

        ecs::add_component(0, local4{0});
        ecs::commit_changes();
        sys.update();
        REQUIRE(0 == ecs::get_component<local4>(0)->c);

        sys.enable();
        sys.update();
        REQUIRE(1 == ecs::get_component<local4>(0)->c);
    }

    SECTION("Groups order systems correctly") {
        struct S1 {};
        struct S2 {};
        struct S3 {};
        struct Sx {};

        // Add systems in reverse order, they should execute in correct order
        int counter = 0;
        ecs::make_system<3>([&counter](S3&) {
            REQUIRE(counter == 3);
            counter++;
        });
        ecs::make_system<2>([&counter](S2&) {
            REQUIRE(counter == 2);
            counter++;
        });
        ecs::make_system<1>([&counter](S1&) {
            REQUIRE(counter == 1);
            counter++;
        });
        ecs::make_system<std::numeric_limits<int>::min()>([&counter](Sx&) {
            REQUIRE(counter == 0);
            counter++;
        });

        ecs::add_component(0, S1{}, S3{}, Sx{}, S2{});
        ecs::update_systems();
    }

    SECTION("Components are passed in the correct order to the system") {
        ecs::detail::_context.reset();

        struct C_Order1 {
            unsigned i;
        };
        struct C_Order2 {
            unsigned j;
        };

        // Add a system to check the order
        ecs::make_system([](C_Order1& o1, C_Order2& o2) { CHECK(o1.i < o2.j); });

        // Add the test components
        ecs::add_component(0, C_Order1{1}, C_Order2{2});

        ecs::update_systems();
    }

    SECTION("Read/write info on systems is correct") {
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
}

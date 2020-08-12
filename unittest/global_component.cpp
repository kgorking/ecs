#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>


TEST_CASE("Global component", "[component][global]") {
    ecs::detail::_context.reset();

    struct test_s {
        ecs_flags(ecs::flag::global);
        int i = 0;
    };

    ecs::detail::_context.reset();

    auto& pst = ecs::get_global_component<test_s>();
    pst.i = 42;

    std::int64_t counter = 0;
    ecs::make_system<ecs::opts::not_parallel>([&counter](test_s const& st, int const&) {
        CHECK(42 == st.i);
        counter++;
    });

    ecs::add_component({0, 2}, int{});
    ecs::commit_changes();

    // Only 1 test_s should exist
    CHECK(1 == ecs::get_component_count<test_s>());

    // Test the content of the entities
    ecs::run_systems();
    CHECK(3 == counter);

    ecs::remove_component<int>({0, 2});
    ecs::commit_changes();
    ecs::run_systems();
    CHECK(3 == counter);
}

#include "catch.hpp"
#include <ecs/ecs.h>

TEST_CASE("Filtering", "[component][system]") {
    ecs::detail::_context.reset();

    ecs::add_component({0, 6}, int());
    ecs::add_component({3, 9}, float());
    ecs::commit_changes();

    ecs::make_system([](ecs::entity_id id, int&) {
        CHECK(id >= 0);
        CHECK(id <= 6);
    });
    ecs::make_system([](ecs::entity_id id, float&) {
        CHECK(id >= 3);
        CHECK(id <= 9);
    });
    ecs::make_system([](ecs::entity_id id, int&, float*) {
        CHECK(id >= 0);
        CHECK(id <= 3);
    });
    ecs::make_system([](ecs::entity_id id, int*, float&) {
        CHECK(id >= 7);
        CHECK(id <= 9);
    });
    ecs::make_system([](ecs::entity_id id, int&, float&) {
        CHECK(id >= 3);
        CHECK(id <= 6);
    });

    // Filtering on non-existant component should run normally
    int no_shorts = 0;
    ecs::make_system([&no_shorts](int&, short*) {
        no_shorts++;
    });

    ecs::run_systems();

    CHECK(no_shorts == ecs::get_entity_count<int>());
}

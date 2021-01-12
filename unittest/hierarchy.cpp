#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>
#include <vector>

using namespace ecs;

void reset() {
    ecs::detail::get_context().reset();
}

TEST_CASE("Hierarchies") {
    SECTION("are traversed correctly") {
        reset();

        //     ______1_________
        //    /      |         \
        //   4       3          2
        //  /|\     /|\       / | \
        // 5 6 7   8 9 10   11  12 13
        // |         |             |
        // 14        15            16

        // The root
        add_component({1}, int{});

        // The children
        add_component(4, parent{1});
        add_component(3, parent{1});
        add_component(2, parent{1});

        // The grandchildren
        add_component({5, 7}, parent{4});
        add_component({8, 10}, parent{3});
        add_component({11, 13}, parent{2});

        // The great-grandchildren
        add_component(14, parent{5});
        add_component(15, parent{9});
        add_component(16, parent{13});

        // The system to verify the traversal order
        std::vector<int> actual_traversal_order;
        make_system<opts::not_parallel>(
            [&actual_traversal_order](entity_id id, parent<>) { actual_traversal_order.push_back(id); });

        update();

        std::vector<int> const expected_traversal_order{2, 11, 12, 13, 16, 3, 8, 9, 15, 10, 4, 5, 14, 6, 7};
        CHECK(expected_traversal_order == actual_traversal_order);
    }

    SECTION("can be built bottoms-up") {
        reset();

        // 14        15            16
        // |         |             |
        // 5 6 7   8 9 10   11  12 13
        //  \|/     \|/       \ | /
        //   4       3          2
        //    \______|_________/
        //           1

        // The great-grandchildren
        add_component(14, parent{5});
        add_component(15, parent{9});
        add_component(16, parent{13});

        // The grandchildren
        add_component({5, 7}, parent{4});
        add_component({8, 10}, parent{3});
        add_component({11, 13}, parent{2});

        // The children
        add_component(4, parent{1});
        add_component(3, parent{1});
        add_component(2, parent{1});

        // The root
        add_component({1}, int{});

        // The system to verify the traversal order
        std::vector<int> actual_traversal_order;
        make_system<opts::not_parallel>(
            [&actual_traversal_order](entity_id id, parent<>) { actual_traversal_order.push_back(id); });

        update();

        std::vector<int> const expected_traversal_order{2, 11, 12, 13, 16, 3, 8, 9, 15, 10, 4, 5, 14, 6, 7};
        CHECK(expected_traversal_order == actual_traversal_order);
    }

    SECTION("can be built in reverse") {
        reset();

        //      ______16________
        //     /      |         \
        //    13      14        15
        //   /| \    /|\        /|\
        // 10 11 12 7 8 9      4 5 6
        //  |         |            |
        //  3         2            1

        // The root
        add_component({16}, int{});

        // The children
        add_component(15, parent{16});
        add_component(14, parent{16});
        add_component(13, parent{16});

        // The grandchildren
        add_component({10, 12}, parent{13});
        add_component({7, 9}, parent{14});
        add_component({4, 6}, parent{15});

        // The great-grandchildren
        add_component(3, parent{10});
        add_component(2, parent{8});
        add_component(1, parent{6});

        // The system to verify the traversal order
        std::vector<int> actual_traversal_order;
        make_system<opts::not_parallel>(
            [&actual_traversal_order](entity_id id, parent<>) { actual_traversal_order.push_back(id); });

        update();

        std::vector<int> const expected_traversal_order{13, 10, 3, 11, 12, 14, 7, 8, 2, 9, 15, 4, 5, 6, 1};
        CHECK(expected_traversal_order == actual_traversal_order);
    }

    SECTION("can extract parent info") {
        reset();

        // The root
        add_component({1}, int{});

        // The children
        add_component(2, parent{1}, short{10});
        add_component(3, parent{1}, long{20});
        add_component(4, parent{1}, float{30});

        // The grandchildren
        add_component({5, 7}, parent{2});   // short children, parent 2 has a short
        add_component({8, 10}, parent{3});  // long children, parent 3 has a long
        add_component({11, 13}, parent{4}); // float children, parent 4 has a float

        // verify parent types
        std::atomic_int count_short = 0, count_long = 0, count_float = 0;
        make_system([&count_short](entity_id id, parent<short> const& p) {
            CHECK((id >= 5 && id <= 7));   // check id value
            CHECK(p.get<short>() == 10); // check parent value
            count_short++;
        });
        make_system([&count_long](entity_id id, parent<long> const& p) {
            CHECK((id >= 8 && id <= 10));
            CHECK(p.get<long>() == 20);
            count_long++;
        });
        make_system([&count_float](entity_id id, parent<float> const& p) {
            CHECK((id >= 11 && id <= 13));
            CHECK(p.get<float>() == 30);
            count_float++;
        });

        update();

        CHECK(count_short == 3);
        CHECK(count_long == 3);
        CHECK(count_float == 3);
    }

    SECTION("can filter on parents") {
        reset();

        add_component({0}, int{});
        add_component({1}, int{}, parent{0});

        // This system is not a hierarchy
        bool filter_works = false;
        make_system([&filter_works](entity_id id, int, parent<>*) { // run on entities with an int and no parent
            CHECK(id == 0);
            filter_works = true;
        });

        update();

        CHECK(filter_works);
    }

    SECTION("can filter on parent subtypes") { // parent<int, float*>
        reset();

        add_component({0}, int{11}, float{});
        add_component({1}, int{22}, parent{0});
        add_component({2}, int{33}, float{}, parent{1});

        // run on entities with an int and a parent with no float
        make_system([](entity_id id, int i, parent<float*> p) {
            CHECK(id == 2);
            CHECK(i == 33);
            CHECK(p.id() == 1);
        });

        update();
    }
}

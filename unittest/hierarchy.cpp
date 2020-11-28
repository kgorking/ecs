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
        make_system<opts::not_parallel>([&actual_traversal_order](entity_id id, parent<> /*p*/) {
            actual_traversal_order.push_back(id);
        });

        update();

        std::vector<int> const expected_traversal_order{4, 5, 14, 6, 7, 3, 8, 9, 15, 10, 2, 11, 12, 13, 16};
        bool const orders_match = (expected_traversal_order == actual_traversal_order);

        REQUIRE(orders_match);
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
        make_system([&count_short](entity_id id, parent<short> p_s) {
            REQUIRE((id >= 5 && id <= 7));      // check id value
            REQUIRE(p_s.get<short>() == 10);    // check parent value
            count_short++;
        });
        make_system([&count_long](entity_id id, parent<long> p_s) {
            REQUIRE((id >= 8 && id <= 10));
            REQUIRE(p_s.get<long>() == 20);
            count_long++;
        });
        make_system([&count_float](entity_id id, parent<float> p_s) {
            REQUIRE((id >= 11 && id <= 13));
            REQUIRE(p_s.get<float>() == 30);
            count_float++;
        });

        update();

        REQUIRE(count_short == 3);
        REQUIRE(count_long == 3);
        REQUIRE(count_float == 3);
    }
}

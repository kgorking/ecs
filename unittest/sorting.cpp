#include "catch.hpp"
#include <ecs/ecs.h>
#include <functional>

int generator(ecs::entity_id) { return rand() % 9; };

TEST_CASE("Sorting") {
    ecs::detail::_context.reset();

    ecs::add_components({0, 9}, generator);
    ecs::commit_changes();

    int test = std::numeric_limits<int>::min();
    auto& asc = ecs::make_system(
        [&test](int const& i) {
            CHECK(test <= i);
            test = i;
        },
        std::less<int>());
    asc.update();

    test = std::numeric_limits<int>::max();
    auto& dec = ecs::make_system(
        [&test](int const& i) {
            CHECK(test >= i);
            test = i;
        },
        std::greater<int>());
    dec.update();

    // modify the components and re-check
    auto& mod = ecs::make_system([](int& i) { i = generator(0); });
    mod.update();

    test = std::numeric_limits<int>::min();
    asc.update();

    test = std::numeric_limits<int>::max();
    dec.update();
}

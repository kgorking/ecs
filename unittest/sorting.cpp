#include "catch.hpp"
#include <ecs/ecs.h>

auto constexpr generator = [](ecs::entity_id) -> int { return rand() % 99; };
auto constexpr sort_asc = [](int const& l, int const& r) { return l < r; };
auto constexpr sort_dec = [](int const& l, int const& r) { return l > r; };

TEST_CASE("Sorting") {
    ecs::detail::_context.reset();

    ecs::add_components({0, 99}, generator);
    ecs::commit_changes();

    int test = std::numeric_limits<int>::min();
    auto& asc = ecs::make_system(
        [&test](int const& i) {
            CHECK(test <= i);
            test = i;
        },
        sort_asc);
    asc.update();

    test = std::numeric_limits<int>::max();
    auto& dec = ecs::make_system(
        [&test](int const& i) {
            CHECK(test >= i);
            test = i;
        },
        sort_dec);
    dec.update();
}

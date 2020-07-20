#include <iostream>

#include <ecs/ecs.h>

auto constexpr printer = [](int const& i) { std::cout << i << ' '; };
auto constexpr generator = [](ecs::entity_id) -> int { return rand() % 9; };

auto constexpr sort_asc = [](int const& l, int const& r) { return l < r; };
auto constexpr sort_des = [](int const& l, int const& r) { return l > r; };
auto constexpr sort_even_odd = [](int const& l, int const& r) {
    // sort evens to the left, odds to the right
    if (l % 2 == 0 && r % 2 != 0)
        return true;
    if (l % 2 != 0 && r % 2 == 0)
        return false;
    return l < r;
};

int main() {
    auto& sys_no_sort = ecs::make_system(printer);
    auto& sys_sort_asc = ecs::make_system(printer, sort_asc);
    auto& sys_sort_des = ecs::make_system(printer, sort_des);
    auto& sys_sort_eo = ecs::make_system(printer, sort_even_odd);

    ecs::add_components({0, 9}, generator);
    ecs::commit_changes();

    sys_no_sort.update();
    std::cout << '\n';
    sys_sort_asc.update();
    std::cout << '\n';
    sys_sort_des.update();
    std::cout << '\n';
    sys_sort_eo.update();
}

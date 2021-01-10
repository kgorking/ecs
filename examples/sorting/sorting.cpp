#include <iostream>
#include <ecs/ecs.h>

auto constexpr printer = [](int const& i) { std::cout << i << ' '; };
auto constexpr generator = [](ecs::entity_id) -> int { return rand() % 9; };

//auto constexpr sort_even_odd = [](int const& l, int const& r) {
bool sort_even_odd(int const& l, int const& r) {
    // sort evens to the left, odds to the right
    if (l % 2 == 0 && r % 2 != 0)
        return true;
    if (l % 2 != 0 && r % 2 == 0)
        return false;
    return l < r;
};

int main() {
    auto& sys_no_sort = ecs::make_system<ecs::opts::not_parallel>(printer);
    auto& sys_sort_asc = ecs::make_system<ecs::opts::not_parallel>(printer, std::less<int>{});
    auto& sys_sort_des = ecs::make_system<ecs::opts::not_parallel>(printer, std::greater<int>{});
    auto& sys_sort_eo = ecs::make_system<ecs::opts::not_parallel>(printer, sort_even_odd);

    ecs::add_component({0, 9}, generator);
    ecs::commit_changes();

    sys_no_sort.run();
    std::cout << '\n';

    sys_sort_asc.run();
    std::cout << '\n';

    sys_sort_des.run();
    std::cout << '\n';

    sys_sort_eo.run();
    std::cout << '\n';
}

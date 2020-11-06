#include <ecs/ecs.h>
#include <iostream>
#include <string>



//      _____1________
//     /     |        \
//    /      |         \
//   2       3          4
//  /|\     /|\       / | \
// 5 6 7   8 9 10   11  12 13


using namespace ecs;

auto constexpr print_root = [](entity_id id, parent*, int &) {
    std::cout << id << '\n';
};
auto constexpr print_child = [](entity_id id, parent parent, int &) {
    std::cout << id << " has parent " << parent.id << '\n';
};

int main() {
    // The root
    add_component({1}, int{});

    // The children
    add_component({2, 4}, parent{1}, int{});

    // The grandchildren
    add_component({5, 7}, parent{2}, int{});
    add_component({8, 10}, parent{3}, int{});
    add_component({11, 13}, parent{4}, int{});

    make_system<opts::not_parallel>(print_root);
    make_system<opts::not_parallel>(print_child);

    update();

    // Should print out
    // 1  2 5 6 7  3 8 9 10  4 11 12 13
}

#include <ecs/ecs.h>
#include <iostream>
#include <string>


//      _____1________
//     /     |        \
//    /      |         \
//   4       3          2
//  /|\     /|\       / | \
// 5 6 7   8 9 10   11  12 13
// |         |             |
// 14        15            16
//
// Should print out
// depth  : 4 5 14 6 7 3 8 9 15 10 2 11 12 13 16
//          
// breadth: 4 3 2 5 6 7 8 9 10 11 12 13 14 15 16


using namespace ecs;

auto constexpr print_child = [](entity_id id, parent parent, int &) {
    //std::cout << id << " has parent " << parent << '\n';
    std::cout << id << ' ';
};

int main() {
    // The root
    add_component({1}, int{});

    // The children
    add_component(4, parent{1}, int{});
    add_component(3, parent{1}, int{});
    add_component(2, parent{1}, int{});

    // The grandchildren
    add_component({5, 7}, parent{4}, int{});
    add_component({8, 10}, parent{3}, int{});
    add_component({11, 13}, parent{2}, int{});

    // The great-grandchildren
    add_component(14, parent{5}, int{});
    add_component(15, parent{9}, int{});
    add_component(16, parent{13}, int{});

    make_system<opts::not_parallel>(print_child);

    update();
}

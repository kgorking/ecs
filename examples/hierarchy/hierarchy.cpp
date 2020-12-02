#include <iostream>
#include <ecs/ecs.h>

using namespace ecs;
using std::cout;

// Print children, filtered on their parent
auto constexpr print_roots          = [](entity_id id, int, parent<>*) { cout << id << ' '; };
auto constexpr print_all_children   = [](entity_id id, parent<> /*p*/) { cout << id << ' '; };
auto constexpr print_short_children = [](entity_id id, parent<short> const& p) { cout << id << '(' << p.get<short>() << ") "; };
auto constexpr print_long_children  = [](entity_id id, parent<long> const& p) { cout << id << '(' << p.get<long>() << ") "; };
auto constexpr print_float_children = [](entity_id id, parent<float> const& p) { cout << id << '(' << p.get<float>() << ") "; };
auto constexpr print_double_children = [](entity_id id, parent<double> const& p) { cout << id << '(' << p.get<double>() << ") "; };

int main() {
    // Print the hierarchies
    cout <<
        "     ______1_________              100--101    \n"
        "    /      |         \\              |    |     \n"
        "   4       3          2            103--102    \n"
        "  /|\\     /|\\       / | \\                      \n"
        " 5 6 7   8 9 10   11  12 13                    \n"
        " |         |             |                     \n"
        " 14        15            16                    \n\n\n";

    // A root
    add_component({1}, int{});

    // The children
    add_component(4, parent{1}, int{}, short{10});
    add_component(3, parent{1}, int{}, long{20});
    add_component(2, parent{1}, int{}, float{30});

    // The grandchildren
    add_component({5, 7}, parent{4}, int{});  // short children, parent 4 has a short
    add_component({8, 10}, parent{3}, int{}); // long children, parent 3 has a long
    add_component({11, 13}, parent{2}, int{});// float children, parent 2 has a float

    // The great-grandchildren
    add_component(14, parent{5}, int{});
    add_component(15, parent{9}, int{});
    add_component(16, parent{13}, int{});

    // second small cyclical tree
    add_component({100}, double{0}, parent{103});
    add_component({101}, double{1}, parent{100});
    add_component({102}, double{2}, parent{101});
    add_component({103}, double{3}, parent{102});

    // Make the systems
    auto& sys_roots = make_system(print_roots);
    auto& sys_all = make_system(print_all_children);
    auto& sys_short = make_system(print_short_children);
    auto& sys_long = make_system(print_long_children);
    auto& sys_float = make_system(print_float_children);
    auto& sys_double = make_system(print_double_children);

    commit_changes();

    // Run the systems
    cout << "All roots        : ";   sys_roots.run();  cout << '\n'; // 1
    cout << "All children     : ";   sys_all.run();    cout << '\n'; // 2-16 100-103
    cout << "short children   : ";   sys_short.run();  cout << '\n'; // 5-7
    cout << "long children    : ";   sys_long.run();   cout << '\n'; // 8-10
    cout << "floating children: ";   sys_float.run();  cout << '\n'; // 11-13
    cout << "double children  : ";   sys_double.run(); cout << '\n'; // 100-103
}

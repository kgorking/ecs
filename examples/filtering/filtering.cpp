#include <ecs/ecs.h>
#include <iostream>

int main() {
    ecs::add_component({0, 6}, int());
    ecs::add_component({3, 9}, float());
    ecs::add_component({2, 3}, short());
    ecs::commit_changes();

    auto& i        = ecs::make_system([](ecs::entity_id id, int&          ) { std::cout << id << ' '; });
    auto& f        = ecs::make_system([](ecs::entity_id id, float&        ) { std::cout << id << ' '; });
    auto& s        = ecs::make_system([](ecs::entity_id id, short&        ) { std::cout << id << ' '; });
    auto& i_no_f   = ecs::make_system([](ecs::entity_id id, int&,   float*) { std::cout << id << ' '; });
    auto& f_no_i   = ecs::make_system([](ecs::entity_id id, int*,   float&) { std::cout << id << ' '; });
    auto& i_f      = ecs::make_system([](ecs::entity_id id, int&,   float&) { std::cout << id << ' '; });
    auto& i_no_s   = ecs::make_system([](ecs::entity_id id, int&,   short*) { std::cout << id << ' '; });
    auto& i_no_f_s = ecs::make_system([](ecs::entity_id id, int&, float*, short*) { std::cout << id << ' '; });

    std::cout << "ints:\n";
    i.update();
    std::cout << "\n\n";

    std::cout << "floats:\n";
    f.update();
    std::cout << "\n\n";

    std::cout << "shorts:\n";
    s.update();
    std::cout << "\n\n";

    std::cout << "ints, no floats:\n";
    i_no_f.update();
    std::cout << "\n\n";

    std::cout << "floats, no ints:\n";
    f_no_i.update();
    std::cout << "\n\n";

    std::cout << "ints & floats:\n";
    i_f.update();
    std::cout << "\n\n";

    std::cout << "ints, no shorts:\n";
    i_no_s.update();
    std::cout << "\n\n";

    std::cout << "ints, no floats, no shorts:\n";
    i_no_f_s.update();
    std::cout << "\n\n";
}

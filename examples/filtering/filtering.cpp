#include <ecs/ecs.h>
#include <iostream>

int main() {
    ecs::add_components({0, 6}, int());
    ecs::add_components({3, 9}, float());
    ecs::add_components({2, 3}, short());
    ecs::commit_changes();

    auto& s_ints            = ecs::make_system([](ecs::entity_id id, int&          ) { std::cout << id << ' '; });
    auto& s_floats          = ecs::make_system([](ecs::entity_id id, float&        ) { std::cout << id << ' '; });
    auto& s_shorts          = ecs::make_system([](ecs::entity_id id, short&        ) { std::cout << id << ' '; });
    auto& s_ints_no_floats  = ecs::make_system([](ecs::entity_id id, int&,   float*) { std::cout << id << ' '; });
    auto& s_floats_no_ints  = ecs::make_system([](ecs::entity_id id, int*,   float&) { std::cout << id << ' '; });
    auto& s_ints_and_floats = ecs::make_system([](ecs::entity_id id, int&,   float&) { std::cout << id << ' '; });
    auto& s_ints_no_f_shorts = ecs::make_system([](ecs::entity_id id, int&, float*, short*) { std::cout << id << ' '; });

    std::cout << "ints:\n";
    s_ints.update();
    std::cout << "\n\n";

    std::cout << "floats:\n";
    s_floats.update();
    std::cout << "\n\n";

    std::cout << "shorts:\n";
    s_shorts.update();
    std::cout << "\n\n";

    std::cout << "ints - floats:\n";
    s_ints_no_floats.update();
    std::cout << "\n\n";

    std::cout << "floats - ints:\n";
    s_floats_no_ints.update();
    std::cout << "\n\n";

    std::cout << "ints & floats:\n";
    s_ints_and_floats.update();
    std::cout << "\n\n";

    std::cout << "ints - floats - shorts:\n";
    s_ints_no_f_shorts.update();
    std::cout << '\n';
}

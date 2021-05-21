#include <ecs/ecs.h>
#include <iostream>

int main() {
    using serial = ecs::opts::not_parallel;

    ecs::runtime ecs;
    ecs.add_component({0, 6}, int());
    ecs.add_component({3, 9}, float());
    ecs.add_component({2, 3}, short());
    ecs.commit_changes();

    auto& i        = ecs.make_system<serial>([](ecs::entity_id id, int&) { std::cout << id << ' '; });
    auto& f        = ecs.make_system<serial>([](ecs::entity_id id, float&        )       { std::cout << id << ' '; });
    auto& s        = ecs.make_system<serial>([](ecs::entity_id id, short&        )       { std::cout << id << ' '; });
    auto& i_no_f   = ecs.make_system<serial>([](ecs::entity_id id, int&,   float*)       { std::cout << id << ' '; });
    auto& f_no_i   = ecs.make_system<serial>([](ecs::entity_id id, int*,   float&)       { std::cout << id << ' '; });
    auto& i_f      = ecs.make_system<serial>([](ecs::entity_id id, int&,   float&)       { std::cout << id << ' '; });
    auto& i_no_s   = ecs.make_system<serial>([](ecs::entity_id id, int&,   short*)       { std::cout << id << ' '; });
    auto& i_no_f_s = ecs.make_system<serial>([](ecs::entity_id id, int&, float*, short*) { std::cout << id << ' '; });

    std::cout << "ints:\n";
    i.run();
    std::cout << "\n\n";

    std::cout << "floats:\n";
    f.run();
    std::cout << "\n\n";

    std::cout << "shorts:\n";
    s.run();
    std::cout << "\n\n";

    std::cout << "ints, no floats:\n";
    i_no_f.run();
    std::cout << "\n\n";

    std::cout << "floats, no ints:\n";
    f_no_i.run();
    std::cout << "\n\n";

    std::cout << "ints & floats:\n";
    i_f.run();
    std::cout << "\n\n";

    std::cout << "ints, no shorts:\n";
    i_no_s.run();
    std::cout << "\n\n";

    std::cout << "ints, no floats, no shorts:\n";
    i_no_f_s.run();
    std::cout << "\n\n";
}

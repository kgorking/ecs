#include <ecs/ecs.h>
#include <iostream>

int main() {
    ecs::add_components({0, 9}, int{0});
    ecs::add_components({6, 9}, float{0});

    ecs::make_system([](ecs::entity_id id, int& i, float*) { std::cout << "ent: " << id << " - " << i << '\n'; });
}

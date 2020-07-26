#include <ecs/ecs.h>

// An example structured like the EnTT example from
// https://github.com/skypjack/entt#code-example
// to compare interfaces

struct position {
    float x;
    float y;
};

struct velocity {
    float dx;
    float dy;
};

int main() {
    float const dt = 1.0f / 60.f;

    ecs::make_system([&dt](position& pos, velocity const& vel) {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
    });

    ecs::make_system([](velocity& vel) {
        vel.dx = 0.;
        vel.dy = 0.;
    });

    ecs::add_component({0, 9}, [](auto id) { return position{id * 1.f, id * 1.f}; });
    ecs::add_component({0, 4}, [](auto id) { return velocity{id * 1.f, id * 1.f}; });

    // Run the systems
    ecs::update();
}

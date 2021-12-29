#include <ecs/ecs.h>

// disable 'unreferenced formal parameter' warnings
#pragma warning(disable : 4100)

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
	ecs::runtime ecs;

	ecs.make_system([](ecs::entity_id id, position& pos, velocity const& vel) { /* ... */ });
	ecs.make_system([](velocity& vel) { /* ... */ });

	ecs.add_component({0, 9}, position{}, velocity{});

	ecs.update();
}

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
	ecs::runtime rt;

	rt.make_system([](ecs::entity_id /*id*/, position& /*pos*/, velocity const& /*vel*/) { /* ... */ });
	rt.make_system([](velocity& /*vel*/) { /* ... */ });

	rt.add_component({0, 9}, position{}, velocity{});

	rt.update();
}

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

template<typename T>
T generator(ecs::entity_id id) {
	auto const fid = (float)id;
	return {fid * .1f, fid * .1f};
}

int main() {
	ecs::runtime rt;

	rt.make_system([](ecs::entity_id /*id*/, position& /*pos*/, velocity const& /*vel*/) { /* ... */ });
	rt.make_system([](velocity& /*vel*/) { /* ... */ });

	rt.add_component_generator({0, 9}, generator<position>);
	rt.add_component_generator({0, 4}, generator<velocity>);

	rt.update();
}

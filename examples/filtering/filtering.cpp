#include <ecs/ecs.h>
#include <iostream>

int main() {
	ecs::runtime rt;
	rt.add_component({0, 20}, int());
	rt.add_component({3, 9}, float());
	rt.add_component({14, 18}, short());

	// Print entities with int, no float or short
	rt.make_system([](ecs::entity_id id, int&, float*, short*) {
		std::cout << id << ' ';
	});
	rt.update();
}

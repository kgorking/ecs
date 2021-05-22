#include <ecs/ecs.h>
#include <iostream>

// This is a slightly altered version of the 'shared_components' example
// where the state_s component is never added to any entities, but is
// still accessible from the system

struct A {};
struct B {};
struct state_s {
	ecs_flags(ecs::flag::global);
	int a = 0;
	int b = 0;
	int total = 0;
};

int main() {
	ecs::runtime ecs;
	ecs.make_system([](A, state_s &state) {
		state.a++;
		state.total++;
	});
	ecs.make_system([](B, state_s &state) {
		state.b++;
		state.total++;
	});
	ecs.make_system([](state_s const &global) {
		std::cout << "  state_s::a:     " << global.a << "\n";
		std::cout << "  state_s::b:     " << global.b << "\n";
		std::cout << "  state_s::total: " << global.total << "\n\n";
	});

	std::cout << "Adding 10 entities with an A component:\n";
	ecs.add_component({0, 9}, A{});

	std::cout << "Adding 10 more entities with a B component:\n\n";
	ecs.add_component({10, 19}, B{});

	ecs.update();

	// Dump some stats
	std::cout << "Number of entities with an A component:      " << ecs.get_entity_count<A>() << "\n";
	std::cout << "Number of entities with a B component:       " << ecs.get_entity_count<B>() << "\n";
	std::cout << "Number of entities with a state_s component: " << ecs.get_entity_count<state_s>() << "\n";
	std::cout << "Number of A components allocated:            " << ecs.get_component_count<A>() << "\n";
	std::cout << "Number of B components allocated:            " << ecs.get_component_count<B>() << "\n";
	std::cout << "Number of state_s components allocated:      " << ecs.get_component_count<state_s>() << "\n";
}

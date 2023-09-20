#include <ecs/ecs.h>
#include <iostream>

// This is a slightly altered version of the 'shared_components' example
// where the state_s component is never added to any entities, but is
// still accessible from the system

struct A {};
struct B {};
struct state_s {
	using ecs_flags = ecs::flags<ecs::global>;
	int a = 0;
	int b = 0;
	int total = 0;
};

int main() {
	ecs::runtime rt;
	rt.make_system([](A, state_s &state) {
		state.a++;
		state.total++;
	});
	rt.make_system([](B, state_s &state) {
		state.b++;
		state.total++;
	});
	rt.make_system([](state_s const &global) {
		std::cout << "  state_s::a:     " << global.a << "\n";
		std::cout << "  state_s::b:     " << global.b << "\n";
		std::cout << "  state_s::total: " << global.total << "\n\n";
	});

	std::cout << "Adding 10 entities with an A component:\n";
	rt.add_component({0, 9}, A{});

	std::cout << "Adding 10 more entities with a B component:\n\n";
	rt.add_component({10, 19}, B{});

	rt.update();

	// Dump some stats
	std::cout << "Number of entities with an A component:      " << rt.get_entity_count<A>() << "\n";
	std::cout << "Number of entities with a B component:       " << rt.get_entity_count<B>() << "\n";
	std::cout << "Number of entities with a state_s component: " << rt.get_entity_count<state_s>() << "\n";
	std::cout << "Number of A components allocated:            " << rt.get_component_count<A>() << "\n";
	std::cout << "Number of B components allocated:            " << rt.get_component_count<B>() << "\n";
	std::cout << "Number of state_s components allocated:      " << rt.get_component_count<state_s>() << "\n";
}

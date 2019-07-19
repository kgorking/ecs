#include <iostream>
#include <ecs/ecs.h>

struct A {};
struct B {};
struct SharedState : ecs::shared {
	int a = 0;
	int b = 0;
	int total = 0;
};

static void print_shared_state()
{
	auto shared = ecs::get_shared_component<SharedState>();
	std::cout << " SharedState - a:     " << shared->a << "\n";
	std::cout << " SharedState - b:     " << shared->b << "\n";
	std::cout << " SharedState - total: " << shared->total << "\n\n";
}

int main()
{
	try {
		std::cout << "Initial state:\n";
		print_shared_state();

		ecs::add_system([](A const&, SharedState &state) { state.a++; state.total++; });
		ecs::add_system([](B const&, SharedState &state) { state.b++; state.total++; });

		std::cout << "Adding 10 entities with an A and SharedState component:\n";
		ecs::entity_range{ 0, 9, A{}, SharedState{} };
		ecs::update_systems(); // run A system
		print_shared_state();

		std::cout << "Adding 10 more entities with a B and SharedState component:\n";
		ecs::entity_range{ 10, 19, B{}, SharedState{} };
		ecs::update_systems(); // run A and B system
		print_shared_state();

		// Dump some stats
		std::cout << "Number of entities with an A components:          " << ecs::get_entity_count<A>() << "\n";
		std::cout << "Number of entities with a B components:           " << ecs::get_entity_count<B>() << "\n";
		std::cout << "Number of entities with a SharedState components: " << ecs::get_entity_count<SharedState>() << "\n";
		std::cout << "Number of A components allocated:                 " << ecs::get_component_count<A>() << "\n";
		std::cout << "Number of B components allocated:                 " << ecs::get_component_count<B>() << "\n";
		std::cout << "Number of SharedState components allocated:       " << ecs::get_component_count<SharedState>() << "\n";
	}
	catch (std::exception const& e) {
		std::cout << e.what() << "\n";
	}
}

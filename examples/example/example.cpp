#include <ecs/ecs.h>
#include <iostream>

// The component
struct greeting {
	char const* msg;
};

int main() {
	ecs::runtime ecs;

	// The system
	ecs.make_system([](greeting const& g) { std::cout << g.msg; });

	// The entities
	ecs.add_component({0, 2}, greeting{"alright "});

	// Run the system on all entities with a 'greeting' component
	ecs.update();
}

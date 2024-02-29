import ecs; //#include <ecs/ecs.h>
#include <iostream>

// The component
struct greeting {
	char const* msg;
};

int main() {
	ecs::runtime rt;

	// The system
	rt.make_system([](greeting const& g) { std::cout << g.msg; });

	// The entities
	rt.add_component({0, 2}, greeting{"alright "});

	// Run the system on all entities with a 'greeting' component
	rt.update();
}

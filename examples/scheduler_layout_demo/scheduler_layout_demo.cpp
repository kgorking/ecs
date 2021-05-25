#define ECS_SCHEDULER_LAYOUT_DEMO
#include <ecs/ecs.h>
#include <iostream>


// Demo used to verify the schedulers layout algorithm
//

template <size_t I> struct type {};

void demo1() {
	ecs::runtime ecs;

	ecs.make_system([](type<0>&) { std::cout << "0 "; });

	// Add the components to an entitiy and run the systems.
	ecs.add_component({0, 9}, type<0>{});
	ecs.update();
	std::cout << "\n";

	// 0 0 0 0 0 0 0 0 0 0 - - - - - - - - - - - - - -
}

void demo2() {
	ecs::runtime ecs;

	ecs.make_system([](type<0>&) { std::cout << "0 "; });
	ecs.make_system([](type<1>&) { std::cout << "1 "; });

	// Add the components to an entitiy and run the systems.
	ecs.add_component({0, 9}, type<0>{}, type<1>{});
	ecs.update();
	std::cout << "\n";

	// 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 1 1 - - - -
}

void demo3() {
	ecs::runtime ecs;

	ecs.make_system([](type<0>&) { std::cout << "0 "; });
	ecs.make_system([](type<1>&, type<0> const&) { std::cout << "1 "; });

	// Add the components to an entitiy and run the systems.
	ecs.add_component({0, 9}, type<0>{}, type<1>{});
	ecs.update();
	std::cout << "\n";

	// 0 0 0 0 0 0 0 0 0 0 - - - - - - - - - - - - - -
	// 1 1 1 1 1 1 1 1 1 1 - - - - - - - - - - - - - -
}

void demo4() {
	ecs::runtime ecs;

	ecs.make_system([](type<0>&) { std::cout << "0 "; });
	ecs.make_system([](type<1>&, type<0> const&) { std::cout << "1 "; });
	ecs.make_system([](type<2>&) { std::cout << "2 "; });

	// Add the components to an entitiy and run the systems.
	ecs.add_component({0, 9}, type<0>{});
	ecs.add_component({0, 4}, type<1>{});
	ecs.add_component({10, 14}, type<2>{});
	ecs.update();
	std::cout << "\n";

	// 0 0 0 0 0 0 0 0 0 0 2 2 2 2 2 - - - - - - - - -
	// 1 1 1 1 1 - - - - - - - - - - - - - - - - - - -
}

void demo5() {
	ecs::runtime ecs;

	ecs.make_system([](type<0>&) { std::cout << "0 "; });
	ecs.make_system([](type<1>&, type<0> const&) { std::cout << "1 "; });

	// Add the components to an entitiy and run the systems.
	ecs.add_component({0, 9}, type<0>{});
	ecs.add_component({4, 9}, type<1>{});
	ecs.update();
	std::cout << "\n";

	// 0 0 0 0 0 0 0 0 0 0 - - - - - - - - - - - - - -
	// - - - - 1 1 1 1 1 1 - - - - - - - - - - - - - -
}

void demo6() {
	ecs::runtime ecs;

	ecs.make_system([](type<0>&) { std::cout << "0 "; });
	ecs.make_system([](type<1>&, type<0> const&) { std::cout << "1 "; });
	ecs.make_system([](type<2>&, type<1> const&) { std::cout << "2 "; });

	// Add the components to an entitiy and run the systems.
	ecs.add_component({0, 9}, type<0>{});
	ecs.add_component({3, 6}, type<1>{});
	ecs.add_component({5, 8}, type<2>{});
	ecs.update();
	std::cout << "\n";

	// 0 0 0 0 0 0 0 0 0 0 - - - - - - - - - - - - - -
	// - - - 1 1 1 1 - - - - - - - - - - - - - - - - -
	// - - - - - 2 2 - - - - - - - - - - - - - - - - -
}

int main() {
	demo1(); // ok
	demo2(); // ok
	demo3(); // ok
	demo4(); // ok
	demo5(); // ok
	demo6(); // ok
}

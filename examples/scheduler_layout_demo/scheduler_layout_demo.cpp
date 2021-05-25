//#define ECS_SCHEDULER_LAYOUT_DEMO
#include <ecs/ecs.h>
#include <iostream>


// Demo used to verify the schedulers layout algorithm
//

template <size_t I> struct type {};

void print(char const* sz, bool& once) {
	if (once) {
		std::cout << sz;
		once = false;
	}
}

void demo1() {
	ecs::runtime ecs;

	ecs.make_system([once = true](type<0>&) mutable { print("0 ", once); });

	ecs.add_component({0, 9}, type<0>{});
	ecs.update();
	std::cout << "\n";
}

void demo2() {
	ecs::runtime ecs;

	ecs.make_system([once = true](type<0>&) mutable { print("0 ", once); });
	ecs.make_system([once = true](type<1>&) mutable { print("1 ", once); });

	ecs.add_component({0, 9}, type<0>{}, type<1>{});
	ecs.update();
	std::cout << "\n";
}

void demo3() {
	ecs::runtime ecs;

	ecs.make_system([once = true](type<0>&) mutable { print("0 ", once); });
	ecs.make_system([once = true](type<1>&, type<0> const&) mutable { print("1 ", once); });

	ecs.add_component({0, 9}, type<0>{}, type<1>{});
	ecs.update();
	std::cout << "\n";
}

void demo4() {
	ecs::runtime ecs;

	ecs.make_system([once = true](type<0>&) mutable { print("0 ", once); });
	ecs.make_system([once = true](type<1>&, type<0> const&) mutable { print("1 ", once); });
	ecs.make_system([once = true](type<2>&) mutable { print("2 ", once); });

	ecs.add_component({0, 9}, type<0>{});
	ecs.add_component({0, 4}, type<1>{});
	ecs.add_component({10, 14}, type<2>{});
	ecs.update();
	std::cout << "\n";
}

void demo5() {
	ecs::runtime ecs;

	ecs.make_system([once = true](type<0>&) mutable { print("0 ", once); });
	ecs.make_system([once = true](type<1>&, type<0> const&) mutable { print("1 ", once); });

	ecs.add_component({0, 9}, type<0>{});
	ecs.add_component({4, 9}, type<1>{});
	ecs.update();
	std::cout << "\n";
}

void demo6() {
	ecs::runtime ecs;

	ecs.make_system([once = true](type<0>&) mutable { print("0 ", once); });
	ecs.make_system([once = true](type<1>&, type<0> const&) mutable { print("1 ", once); });
	ecs.make_system([once = true](type<2>&, type<1> const&) mutable { print("2 ", once); });

	ecs.add_component({0, 9}, type<0>{});
	ecs.add_component({3, 6}, type<1>{});
	ecs.add_component({5, 8}, type<2>{});
	ecs.update();
	std::cout << "\n";
}

void demo7() {
	ecs::runtime ecs;

	ecs.make_system([once = true](type<0>&) mutable { print("0 ", once); });

	ecs.add_component({0, 9000}, type<0>{});
	ecs.update();
	std::cout << "\n";
}

void demo8() {
	ecs::runtime ecs;

	ecs.make_system([once = true](type<0>&) mutable { print("0 ", once); });
	ecs.make_system([once = true](type<1>&, type<0> const&) mutable { if (once) {std::cout << "1 "; once = false; } });
	ecs.make_system([once = true](type<2>&, type<1> const&) mutable { if (once) {std::cout << "2 "; once = false; } });

	ecs.add_component({0, 9000}, type<0>{});
	ecs.add_component({3000, 6000}, type<1>{});
	ecs.add_component({5000, 8000}, type<2>{});
	ecs.update();
	std::cout << "\n";

	// 0 0 0 0 0 0 0 0 0 0 - - - - - - - - - - - - - -
	// - - - 1 1 1 1 - - - - - - - - - - - - - - - - -
	// - - - - - 2 2 - - - - - - - - - - - - - - - - -
}

void demo9() {
	ecs::runtime ecs;

	struct sched_test {};

	// Create 100 systems that will execute concurrently,
	// because they have no dependencies on each other.
	for (int i = 0; i < 100; i++) {
		ecs.make_system([](sched_test const&) { std::cout << "- "; });
	}

	// Create a system that will only run after the 100 systems.
	// It can not run concurrently with the other 100 systems,
	// because it has a read/write dependency on all 100 systems.
	ecs.make_system([](sched_test&) {
		// Expects(100 == counter);
		std::cout << "X ";
	});


	// Create another 100 systems that will execute concurrently,
	// because they have no dependencies on each other.
	for (int i = 0; i < 100; i++) {
		ecs.make_system([](sched_test const&) { std::cout << "| "; });
	}
	// Add a component to trigger the systems
	ecs.add_component(0, sched_test{});
	ecs.commit_changes();

	// Run the systems
	for (int i = 0; i < 1; i++) {
		ecs.run_systems();
	}
}


int main() {
	//demo1(); // ok
	//demo2(); // ok
	//demo3(); // ok
	//demo4(); // ok
	//demo5(); // ok
	//demo6(); // ok
	//demo7(); // ok
	//demo8(); // ok
	demo9();
}

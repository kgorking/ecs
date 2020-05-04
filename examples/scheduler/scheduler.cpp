#include <ecs/ecs.h>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

// A simple example that creates 6 systems with dependencies on 3 components.
//
// Prints out each systems dependencies, which
// can then be verified when the systems are run.
//
// Systems without dependencies are executed concurrently,
// while system with dependencies will only be executed
// after other systems are done with them.

template <size_t I>
struct type { };

int main() {
	std::cout << std::boolalpha;

	// creates lane 0
	auto & sys1 = ecs::make_system([](type<0>& , type<1> const& ) {
		std::cout << "sys1\n";
		std::this_thread::sleep_for(20ms); // simulate works
	});
	std::cout << "sys1: " << sys1.get_signature() << '\n';

	// add to lane 0
	auto & sys2 = ecs::make_system([](type<1>& ) { 
		std::cout << "sys2\n";
		std::this_thread::sleep_for(20ms);
	});
	std::cout << "sys2: " << sys2.get_signature() << '\n';
	std::cout << " depends on sys1? " << sys2.depends_on(&sys1) << '\n';

	// creates lane 1
	auto & sys3 = ecs::make_system([](type<2>&) { 
		std::cout << "sys3\n";
		std::this_thread::sleep_for(20ms);
	});
	std::cout << "sys3: " << sys3.get_signature() << '\n';
	std::cout << " depends on sys1? " << sys3.depends_on(&sys1) << '\n';
	std::cout << " depends on sys2? " << sys3.depends_on(&sys2) << '\n';

	// add to lane 0
	auto & sys4 = ecs::make_system([](type<0> const&) { 
		std::cout << "sys4\n";
		std::this_thread::sleep_for(20ms);
	});
	std::cout << "sys4: " << sys4.get_signature() << '\n';
	std::cout << " depends on sys1? " << sys4.depends_on(&sys1) << '\n';
	std::cout << " depends on sys2? " << sys4.depends_on(&sys2) << '\n';
	std::cout << " depends on sys3? " << sys4.depends_on(&sys3) << '\n';

	// add to lane 0+1
	auto & sys5 = ecs::make_system([](type<2>&, type<0> const&) { 
		std::cout << "sys5\n";
		std::this_thread::sleep_for(20ms);
	});
	std::cout << "sys5: " << sys5.get_signature() << '\n';
	std::cout << " depends on sys1? " << sys5.depends_on(&sys1) << '\n';
	std::cout << " depends on sys2? " << sys5.depends_on(&sys2) << '\n';
	std::cout << " depends on sys3? " << sys5.depends_on(&sys3) << '\n';
	std::cout << " depends on sys4? " << sys5.depends_on(&sys4) << '\n';

	// add to lane 1
	auto & sys6 = ecs::make_system([](type<2> const&) { 
		std::cout << "sys6\n";
		std::this_thread::sleep_for(20ms);
	});
	std::cout << "sys6: " << sys6.get_signature() << '\n';
	std::cout << " depends on sys1? " << sys6.depends_on(&sys1) << '\n';
	std::cout << " depends on sys2? " << sys6.depends_on(&sys2) << '\n';
	std::cout << " depends on sys3? " << sys6.depends_on(&sys3) << '\n';
	std::cout << " depends on sys4? " << sys6.depends_on(&sys4) << '\n';
	std::cout << " depends on sys5? " << sys6.depends_on(&sys5) << '\n';

	std::cout << "\nrunning systems:\n";
	ecs::add_components({0, 2}, type<0>{}, type<1>{}, type<2>{});
	ecs::commit_changes();
	ecs::run_systems();
}

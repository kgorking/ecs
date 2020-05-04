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
	std::cout << "creating systems:\n";


	auto & sys1 = ecs::make_system([](type<0>& , type<1> const& ) {
		std::cout << "1 ";
		std::this_thread::sleep_for(20ms); // simulate work
	});
	std::cout << "1 - " << sys1.get_signature() << '\n';


	auto & sys2 = ecs::make_system([](type<1>& ) { 
		std::cout << "2 ";
		std::this_thread::sleep_for(20ms);
	});
	std::cout << "2 - " << sys2.get_signature() << '\n';
	std::cout << " depends on 1? " << sys2.depends_on(&sys1) << '\n';


	auto & sys3 = ecs::make_system([](type<2>&) { 
		std::cout << "3 ";
		std::this_thread::sleep_for(20ms);
	});
	std::cout << "3 - " << sys3.get_signature() << '\n';
	std::cout << " depends on 1? " << sys3.depends_on(&sys1) << '\n';
	std::cout << " depends on 2? " << sys3.depends_on(&sys2) << '\n';


	auto & sys4 = ecs::make_system([](type<0> const&) { 
		std::cout << "4 ";
		std::this_thread::sleep_for(20ms);
	});
	std::cout << "4 - " << sys4.get_signature() << '\n';
	std::cout << " depends on 1? " << sys4.depends_on(&sys1) << '\n';
	std::cout << " depends on 2? " << sys4.depends_on(&sys2) << '\n';
	std::cout << " depends on 3? " << sys4.depends_on(&sys3) << '\n';


	auto & sys5 = ecs::make_system([](type<2>&, type<0> const&) { 
		std::cout << "5 ";
		std::this_thread::sleep_for(20ms);
	});
	std::cout << "5 - " << sys5.get_signature() << '\n';
	std::cout << " depends on 1? " << sys5.depends_on(&sys1) << '\n';
	std::cout << " depends on 2? " << sys5.depends_on(&sys2) << '\n';
	std::cout << " depends on 3? " << sys5.depends_on(&sys3) << '\n';
	std::cout << " depends on 4? " << sys5.depends_on(&sys4) << '\n';


	auto & sys6 = ecs::make_system([](type<2> const&) { 
		std::cout << "6 ";
		std::this_thread::sleep_for(20ms);
	});
	std::cout << "6 - " << sys6.get_signature() << '\n';
	std::cout << " depends on 1? " << sys6.depends_on(&sys1) << '\n';
	std::cout << " depends on 2? " << sys6.depends_on(&sys2) << '\n';
	std::cout << " depends on 3? " << sys6.depends_on(&sys3) << '\n';
	std::cout << " depends on 4? " << sys6.depends_on(&sys4) << '\n';
	std::cout << " depends on 5? " << sys6.depends_on(&sys5) << '\n';


	std::cout << "\nrunning systems on 5 entities:\n";
	ecs::add_components({0, 4}, type<0>{}, type<1>{}, type<2>{});
	ecs::update_systems();
}

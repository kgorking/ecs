#include <ecs/ecs.h>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

// A small example that creates 6 systems with dependencies on 3 components.
//
// Systems without dependencies are executed concurrently,
// while systems with dependencies will only be executed
// after other systems are done with them.

template <size_t I>
struct type {};

int main() {
	ecs::runtime rt;

	std::cout << std::boolalpha;
	std::cout << "creating systems:\n";

	// The runtime assumes that type 0 is written to and type 1 is only read from.
	std::cout << "sys1 (type<0>&, type<1> const&)\n";
	rt.make_system([](type<0>&, type<1> const&) { 
		std::cout << "1 ";
		std::this_thread::sleep_for(20ms); // simulate work
	});

	// Writes to type 1.
	// This system must not execute until after sys1 is done,
	// in order to avoid race conditions.
	std::cout << "sys2 (type<1>&)\n";
	rt.make_system([](type<1>&) {
		std::cout << "2 ";
		std::this_thread::sleep_for(20ms);
	});

	// Writes to type 2.
	// This has no dependencies on type 0 or 1,
	// so it can be run concurrently with sys1 and sys2.
	std::cout << "sys3 (type<2>&)\n";
	rt.make_system([](type<2>&) {
		std::cout << "3 ";
		std::this_thread::sleep_for(20ms);
	});

	// Reads from type 0.
	// Must not execute until sys1 is done.
	std::cout << "sys4 (type<0> const&)\n";
	rt.make_system([](type<0> const&) {
		std::cout << "4 ";
		std::this_thread::sleep_for(20ms);
	});

	// Writes to type 2 and reads from type 0.
	// Must not execute until sys3 and sys1 are done.
	std::cout << "sys5 (type<2>&, type<0> const&)\n";
	rt.make_system([](type<2>&, type<0> const&) {
		std::cout << "5 ";
		std::this_thread::sleep_for(20ms);
	});

	// Reads from type 2.
	// Must not execute until sys5 is done.
	std::cout << "sys6 (type<2> const&)\n";
	rt.make_system([](type<2> const&) {
		std::cout << "6 ";
		std::this_thread::sleep_for(20ms);
	});

	// Add the components to an entity and run the systems.
	std::cout << "\nrunning systems on 5 entities:\n";
	rt.add_component({0, 4}, type<0>{}, type<1>{}, type<2>{});
	rt.update();
	std::cout << '\n';
}

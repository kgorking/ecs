#include <ecs/ecs.h>
#include <iostream>


// A small example that creates 6 systems with dependencies on 3 components.
//
// Prints out each systems dependencies, which
// can then be verified when the systems are run.
//
// Systems without dependencies are executed concurrently,
// while systems with dependencies will only be executed
// after other systems are done with them.

//           |--------------|
//           |---------|    |
// 1----2    3    4    5----6
// |--------------|    |
// |-------------------|
//
// sys1 (write type<0>, read type<1>)
// 
// sys2 (write type<1>)
//  depends on 1? true
// 
// sys3 (write type<2>)
//  depends on 1? false
//  depends on 2? false
// 
// sys4 (read type<0>)
//  depends on 1? true
//  depends on 2? false
//  depends on 3? false
// 
// sys5 (write type<2>, read type<0>)
//  depends on 1? true
//  depends on 2? false
//  depends on 3? true
//  depends on 4? false
// 
// sys6 (read type<2>)
//  depends on 1? false
//  depends on 2? false
//  depends on 3? true
//  depends on 4? false
//  depends on 5? true

// 1 2 4 3 5 6

template <size_t I>
struct type {};

int main() {
	ecs::runtime ecs;

	std::cout << std::boolalpha;
	std::cout << "creating systems:\n";

	//
	// Assumes that type 0 is writte to, and type 1 is only read from.
	auto const& sys1 = ecs.make_system([](type<0>&, type<1> const&) {
		std::cout << "1 ";
	});
	std::cout << "\nsys1 (write type<0>, read type<1>)\n";

	//
	// Writes to type 1. This system must not execute until after sys1 is done.
	// None of the following system use type 1, so sys2 can run parallel with
	// all of them.
	auto const& sys2 = ecs.make_system([](type<1>&) {
		std::cout << "2 ";
	});
	std::cout << "\nsys2 (write type<1>)\n";
	std::cout << " depends on 1? " << sys2.depends_on(&sys1) << '\n';

	//
	// Writes to type 2. This has no dependencies on type 0 or 1, so it can be run
	// concurrently with sys1 and sys2.
	auto const& sys3 = ecs.make_system([](type<2>&) {
		std::cout << "3 ";
	});
	std::cout << "\nsys3 (write type<2>)\n";
	std::cout << " depends on 1? " << sys3.depends_on(&sys1) << '\n';
	std::cout << " depends on 2? " << sys3.depends_on(&sys2) << '\n';

	//
	// Reads from type 0. Must not execute until sys1 is done.
	auto const& sys4 = ecs.make_system([](type<0> const&) {
		std::cout << "4 ";
	});
	std::cout << "\nsys4 (read type<0>)\n";
	std::cout << " depends on 1? " << sys4.depends_on(&sys1) << '\n';
	std::cout << " depends on 2? " << sys4.depends_on(&sys2) << '\n';
	std::cout << " depends on 3? " << sys4.depends_on(&sys3) << '\n';

	//
	// Systems that can run parallel to all the other systems.
	auto const& sys7 = ecs.make_system([](type<7>&) {
		std::cout << "7 ";
	});
	auto const& sys8 = ecs.make_system([](type<7> const&) {
		std::cout << "8 ";
	});

	//
	// Writes to type 2 and reads from type 0.
	// Must not execute until after sys3 and sys1 is done.
	auto const& sys5 = ecs.make_system([](type<2>&, type<0> const&) {
		std::cout << "5 ";
	});
	std::cout << "\nsys5 (write type<2>, read type<0>)\n";
	std::cout << " depends on 1? " << sys5.depends_on(&sys1) << '\n';
	std::cout << " depends on 2? " << sys5.depends_on(&sys2) << '\n';
	std::cout << " depends on 3? " << sys5.depends_on(&sys3) << '\n';
	std::cout << " depends on 4? " << sys5.depends_on(&sys4) << '\n';

	//
	// Reads from type 2. Must not execute until sys5 is done.
	auto const& sys6 = ecs.make_system([](type<2> const&) {
		std::cout << "6 ";
	});
	std::cout << "\nsys6 (read type<2>)\n";
	std::cout << " depends on 1? " << sys6.depends_on(&sys1) << '\n';
	std::cout << " depends on 2? " << sys6.depends_on(&sys2) << '\n';
	std::cout << " depends on 3? " << sys6.depends_on(&sys3) << '\n';
	std::cout << " depends on 4? " << sys6.depends_on(&sys4) << '\n';
	std::cout << " depends on 5? " << sys6.depends_on(&sys5) << '\n';

	//
	// Add the components to an entitiy and run the systems.
	std::cout << "\nrunning systems on 10 entities with all three types:\n";
	ecs.add_component({0, 9}, type<0>{});
	ecs.add_component({4, 9}, type<1>{});
	ecs.add_component({7, 9}, type<2>{});
	ecs.add_component({0, 9}, type<7>{});
	ecs.update();
	std::cout << '\n';
}

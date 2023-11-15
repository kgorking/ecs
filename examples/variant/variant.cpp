#include <ecs/ecs.h>
#include <iostream>

// Prints out 'A B C'.
// Without the variants it would print 'A AB ABC'
// If 'C' was also a variant of 'A', it prints 'A B BC'

struct A { };
struct B { using variant_of = A; };
struct C { using variant_of = B; };

int main() {
	ecs::runtime rt;

	rt.make_system([](A) { std::cout << 'A'; });
	rt.make_system([](B) { std::cout << 'B'; });
	rt.make_system([](C) { std::cout << 'C'; });

	// Print 'A'
	rt.add_component(0, A{});
	rt.update();
	std::cout << ' ';

	// Print 'B'
	rt.add_component(0, B{});
	rt.update();
	std::cout << ' ';

	// Print 'C'
	rt.add_component(0, C{});
	rt.update();
}

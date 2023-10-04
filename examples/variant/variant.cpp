#include <ecs/ecs.h>
#include <ecs/detail/variant.h>
#include <iostream>

// Prints out 'ACB'

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

	// Print 'C'
	rt.add_component(0, C{});
	rt.update();

	// Print 'B'
	rt.add_component(0, B{});
	rt.update();

	// Print nothing
	rt.remove_component<B>(0);
	rt.update();

	//rt.add_component(0, A{}, B{}, C{}); 
}

#include <ecs/ecs.h>
#include <cstdlib>
#include <cstdio>

// component generator that returns a random character between 0 and 9
char generator(ecs::entity_id) {
	return '0' + (std::rand() % 10);
}

// system that prints a character
void printer(char c) {
	std::putchar(c);
}

// sort evens to the left, odds to the right
bool sort_even_odd(char const& l, char const& r) {
	if (l % 2 == 0 && r % 2 != 0)
		return true;
	if (l % 2 != 0 && r % 2 == 0)
		return false;
	return l < r;
}

int main() {
	ecs::runtime rt;

	rt.add_component_generator({0, 29}, generator);
	rt.add_component(30, ' ');

	rt.make_system(printer);
	rt.make_system(printer, sort_even_odd);

	rt.update();
}

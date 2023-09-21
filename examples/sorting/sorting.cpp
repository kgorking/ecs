#include <ecs/ecs.h>
#include <iostream>

// generator that returns a random number between 0 and 9
int generator(ecs::entity_id) {
	return rand() % 10;
}

// system that prints an integer
void printer(int i) {
	std::cout << i << ' ';
}

// sorting-function that also moves even numbers to the front
bool sort_even_odd(int const& l, int const& r) {
	// sort evens to the left, odds to the right
	if (l % 2 == 0 && r % 2 != 0)
		return true;
	if (l % 2 != 0 && r % 2 == 0)
		return false;
	return l < r;
}

int main() {
	ecs::runtime rt;
	rt.add_component_generator({0, 29}, generator);
	rt.make_system(printer, sort_even_odd);
	rt.update();
}

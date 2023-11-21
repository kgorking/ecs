#include <ecs/ecs.h>
#include <iostream>
#include <string_view>

using namespace std::string_view_literals;

struct is_funny {
	using ecs_flags = ecs::flags<ecs::tag>;
};
struct dad { std::string_view name; };
struct kid { std::string_view name; };

int main() {
	ecs::runtime rt;

	// Add 4 dads
	dad const dads[] = {{"Bill"sv}, {"Fred"sv}, {"Andy"sv}, {"Jeff"sv}};
	rt.add_component_span({0, 3}, dads);

	// Mark 2 of them as funny
	rt.add_component(0, is_funny{});
	rt.add_component(2, is_funny{});

	// Add 6 kids
	kid const kids[] = {{"Olivia"sv}, {"Emma"sv}, {"Charlotte"sv}, {"Amelia"sv}, {"Sophia"sv}, {"Isabella"sv}};
	rt.add_component_span({10, 15}, kids);

	// Set up relationships
	ecs::detail::parent_id const parents[] = {0, 0, 1, 2, 2, 3};
	rt.add_component_span({10, 15}, parents);

	// Create a system that prints which dads are funny
	rt.make_system([](kid const& k, ecs::parent<is_funny, dad> parent) {
		std::cout << k.name << "'s dad " << parent.get<dad>().name << " is funny\n";
	});

	// Create another system that prints which dads are NOT funny
	// Uses a filter in the parent (is_funny*)
	rt.make_system([](kid const& k, ecs::parent<is_funny*, dad> parent) {
		std::cout << k.name << "'s dad " << parent.get<dad>().name << " is NOT funny\n";
	});

	// Run it
	rt.update();
}

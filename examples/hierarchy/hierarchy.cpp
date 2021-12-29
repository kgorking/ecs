#include <ecs/ecs.h>
#include <iostream>

using std::cout;

// Print children, filtered on their parent
auto constexpr print_roots = [](ecs::entity_id id, double, ecs::parent<> *) { cout << id << ' '; };
auto constexpr print_all_children = [](ecs::entity_id id, ecs::parent<> /*p*/) { cout << id << ' '; };
auto constexpr print_short_children = [](ecs::entity_id id, ecs::parent<short> const &p) { cout << id << '(' << p.get<short>() << ") "; };
auto constexpr print_long_children = [](ecs::entity_id id, ecs::parent<long> const &p) { cout << id << '(' << p.get<long>() << ") "; };
auto constexpr print_float_children = [](ecs::entity_id id, ecs::parent<float> const &p) { cout << id << '(' << p.get<float>() << ") "; };
auto constexpr print_double_children = [](ecs::entity_id id, ecs::parent<double> const &p) {
	cout << id << '(' << p.get<double>() << ") ";
};

int main() {
	ecs::runtime ecs;

	// Print the hierarchies
	cout << "     ______1_________              100-101    \n"
			"    /      |         \\                  |     \n"
			"   4       3          2            103-102    \n"
			"  /|\\     /|\\       / | \\                      \n"
			" 5 6 7   8 9 10   11  12 13                    \n"
			" |         |             |                     \n"
			" 14        15            16                    \n\n\n";

	// A root
	ecs.add_component({1}, double{1.23});

	// The children
	ecs.add_component(4, ecs::parent{1}, int{}, short{10});
	ecs.add_component(3, ecs::parent{1}, int{}, long{20});
	ecs.add_component(2, ecs::parent{1}, int{}, float{30});

	// The grandchildren
	ecs.add_component({5, 7}, ecs::parent{4}, int{});	// short children, parent 4 has a short
	ecs.add_component({8, 10}, ecs::parent{3}, int{});	// long children, parent 3 has a long
	ecs.add_component({11, 13}, ecs::parent{2}, int{}); // float children, parent 2 has a float

	// The great-grandchildren
	ecs.add_component(14, ecs::parent{5}, int{});
	ecs.add_component(15, ecs::parent{9}, int{});
	ecs.add_component(16, ecs::parent{13}, int{});

	// second small tree
	ecs.add_component({100}, double{0});
	ecs.add_component({101}, double{1}, ecs::parent{100});
	ecs.add_component({102}, double{2}, ecs::parent{101});
	ecs.add_component({103}, double{3}, ecs::parent{102});

	ecs.commit_changes();

	// Run the systems
	using serial = ecs::opts::not_parallel;
	cout << "All roots        : ";
	auto& sys_roots = ecs.make_system<serial>(print_roots);
	sys_roots.run();
	cout << '\n'; // 1
	cout << "All children     : ";
	auto& sys_all = ecs.make_system<serial>(print_all_children);
	sys_all.run();
	cout << '\n'; // 2-16 100-103
	cout << "short children   : ";
	auto &sys_short = ecs.make_system<serial>(print_short_children);
	sys_short.run();
	cout << '\n'; // 5-7
	cout << "long children    : ";
	auto &sys_long = ecs.make_system<serial>(print_long_children);
	sys_long.run();
	cout << '\n'; // 8-10
	cout << "floating children: ";
	auto &sys_float = ecs.make_system<serial>(print_float_children);
	sys_float.run();
	cout << '\n'; // 11-13
	cout << "double children  : ";
	auto &sys_double = ecs.make_system<serial>(print_double_children);
	sys_double.run();
	cout << '\n'; // 100-103
}

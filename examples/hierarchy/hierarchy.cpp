
#ifndef __cpp_lib_format
#include <iostream>
int main() {
	std::cout << "This example requires <format>\n";
}
#else
#include <ecs/ecs.h>
#include <iostream>
#include <array>
#include <format>

void print_trees(ecs::runtime& rt) {
	const auto f = [&](int i) {
		return *rt.get_component<int>(i);
	};

	constexpr auto tree_fmt_sz = R"(    {}        {}          {}
  / | \    / | \      / | \
 {}  {}  {}  {}  {}  {}    {}  {}  {}
 |           |             |
 {}           {}             {}

)";
	//clang-format off
	std::cout << std::format(tree_fmt_sz, 
		f(4), f(3), f(2),

		f(5), f(6), f(7),   f(8), f(9), f(10),   f(11), f(12), f(13),

		f(14), f(15), f(16));
	//clang-format on
}


int main() {
	using namespace ecs::opts;
	ecs::runtime rt;

	/* Creates the following trees, with ids shown as their nodes

		   4       3          2
		  /|\     /|\       / | \
		 5 6 7   8 9 10   11  12 13
		 |         |             |
		 14        15            16
	*/

	// The node values
	rt.add_component({2, 16}, int{1});

	// The parents of the children
	constexpr std::array<ecs::detail::parent_id, 12> parents{4, 4, 4, 3, 3, 3, 2, 2, 2, 5, 9, 13};
	rt.add_component_span({5, 16}, parents);


	rt.commit_changes();

	std::cout << "Trees before update:\n";
	print_trees(rt);


	std::cout << "Add parents value to children:\n";
	auto& adder = rt.make_system<manual_update>([](int& i, ecs::parent<int> const& p) {
		i += p.get<int>();
	});
	adder.run();
	print_trees(rt);


	std::cout << "Reset tree with new values:\n";
	auto& reset = rt.make_system<manual_update>([](int& i) {
		i = 2;
	});
	reset.run();
	print_trees(rt);


	std::cout << "Subtract parents value from children:\n";
	auto& subber = rt.make_system<manual_update>([](int& i, ecs::parent<int> const& p) {
		i -= p.get<int>();
	});
	subber.run();
	print_trees(rt);


	std::cout << "Add childrens value to parents:\n";
	auto& inv_adder = rt.make_system<manual_update>([](int& i, ecs::parent<int> const& p) {
		p.get<int>() += i;
	});
	inv_adder.run();
	print_trees(rt);


	std::cout << "Print ids in traversal order:\n";
	auto& print_ids = rt.make_system<manual_update, not_parallel>([](ecs::entity_id id, int) {
		std::cout << id << ' ';
	});
	print_ids.run();
	std::cout << '\n';

	std::cout << "Print parent ids in traversal order:\n";
	auto& print_parent_ids = rt.make_system<manual_update, not_parallel>([](int, ecs::parent<> p) {
		std::cout << p.id() << ' ';
	});
	print_parent_ids.run();
	std::cout << '\n';
}
#endif

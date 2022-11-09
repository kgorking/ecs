#include <ecs/ecs.h>
#include <iostream>

auto constexpr printer = [](int const& i) {
	std::cout << i << ' ';
};
auto constexpr generator = [](ecs::entity_id) -> int {
	return rand() % 9;
};

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

	using namespace ecs::opts;

	auto& sys_no_sort = rt.make_system<not_parallel, manual_update>(printer);
	auto& sys_sort_asc = rt.make_system<not_parallel, manual_update>(printer, std::less<int>{});
	auto& sys_sort_des = rt.make_system<not_parallel, manual_update>(printer, std::greater<int>{});
	auto& sys_sort_eo = rt.make_system<not_parallel, manual_update>(printer, sort_even_odd);

	rt.add_component_generator({0, 9}, generator);
	rt.commit_changes();

	std::cout << "Unsorted:   ";
	sys_no_sort.run();
	std::cout << '\n';

	std::cout << "Ascending:  ";
	sys_sort_asc.run();
	std::cout << '\n';

	std::cout << "Descending: ";
	sys_sort_des.run();
	std::cout << '\n';

	std::cout << "even/odd:   ";
	sys_sort_eo.run();
	std::cout << '\n';
}

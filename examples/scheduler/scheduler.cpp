#include <ecs/ecs.h>
#include <ecs/system_scheduler.h>
#include <iostream>

struct position {
	float x;
	float y;
};

struct velocity {
	float dx;
	float dy;
};

int main() {
	// creates lane 0
	auto & sys1 = ecs::make_system([](position& , velocity const& ) { });
	auto & sys2 = ecs::make_system([](velocity& ) { });

	// creates lane 1
	auto & sys3 = ecs::make_system([](int&) { });

	// add to lane 0
	auto & sys4 = ecs::make_system([](position const&) { });

	// add to lane 0+1
	auto & sys5 = ecs::make_system([](int&, position const&) { });

	// add to lane 1
	auto & sys6 = ecs::make_system([](int const&) { });

	// 1 4 5
	// 1 2
	// 3 5 6

	ecs::add_components(0, position{}, velocity{}, int{});
	ecs::commit_changes();

	ecs::detail::system_scheduler ss;
	ss.insert(&sys1);
	ss.insert(&sys2);
	ss.insert(&sys3);
	ss.insert(&sys4);
	ss.insert(&sys5);
	ss.insert(&sys6);
	std::cout << '\n';

	ss.run();

	std::cout << '\n';
	ss.print_lanes();
}

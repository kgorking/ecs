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

	// add to lane 0
	auto & sys2 = ecs::make_system([](velocity& ) { });

	// creates lane 1
	auto & sys3 = ecs::make_system([](int&) { });

	// add to lane 0
	auto & sys4 = ecs::make_system([](position const&) { });

	// add to lane 0+1
	auto & sys5 = ecs::make_system([](int&, position const&) { });

	// add to lane 1
	auto & sys6 = ecs::make_system([](int const&) { });


	ecs::add_components(0, position{}, velocity{}, int{});
	ecs::commit_changes();

	std::cout << '\n';
	std::cout << '\n';
	ecs::run_systems();

	//std::cout << '\n';
	//ss.print_lanes();
}

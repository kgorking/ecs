#include <ecs/ecs.h>
#include <iostream>

int main() {
	ecs::runtime rt;
	rt.add_component({0, 6}, int());
	rt.add_component({3, 9}, float());
	rt.add_component({2, 3}, short());
	rt.commit_changes();

	using namespace ecs::opts;
	auto &i = rt.make_system<not_parallel, manual_update>([](ecs::entity_id id, int &) { std::cout << id << ' '; });
	auto &f = rt.make_system<not_parallel, manual_update>([](ecs::entity_id id, float &) { std::cout << id << ' '; });
	auto &s = rt.make_system<not_parallel, manual_update>([](ecs::entity_id id, short &) { std::cout << id << ' '; });
	auto &i_no_f = rt.make_system<not_parallel, manual_update>([](ecs::entity_id id, int &, float *) { std::cout << id << ' '; });
	auto &f_no_i = rt.make_system<not_parallel, manual_update>([](ecs::entity_id id, int *, float &) { std::cout << id << ' '; });
	auto &i_f = rt.make_system<not_parallel, manual_update>([](ecs::entity_id id, int &, float &) { std::cout << id << ' '; });
	auto &i_no_s = rt.make_system<not_parallel, manual_update>([](ecs::entity_id id, int &, short *) { std::cout << id << ' '; });
	auto &i_no_f_s = rt.make_system<not_parallel, manual_update>([](ecs::entity_id id, int &, float *, short *) { std::cout << id << ' '; });

	std::cout << "ints:\n";
	i.run();
	std::cout << "\n\n";

	std::cout << "floats:\n";
	f.run();
	std::cout << "\n\n";

	std::cout << "shorts:\n";
	s.run();
	std::cout << "\n\n";

	std::cout << "ints, no floats:\n";
	i_no_f.run();
	std::cout << "\n\n";

	std::cout << "floats, no ints:\n";
	f_no_i.run();
	std::cout << "\n\n";

	std::cout << "ints & floats:\n";
	i_f.run();
	std::cout << "\n\n";

	std::cout << "ints, no shorts:\n";
	i_no_s.run();
	std::cout << "\n\n";

	std::cout << "ints, no floats, no shorts:\n";
	i_no_f_s.run();
	std::cout << "\n\n";
}

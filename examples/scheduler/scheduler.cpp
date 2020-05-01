#include <ecs/ecs.h>
#include <ecs/system_scheduler.h>

struct position {
	float x;
	float y;
};

struct velocity {
	float dx;
	float dy;
};

int main() {
	auto & sys1 = ecs::make_system([](position& , velocity const& ) { });
	auto & sys2 = ecs::make_system([](velocity& ) { });
	auto & sys3 = ecs::make_system([](int&) {});

	ecs::detail::system_scheduler ss;
	ss.insert(sys1);
	ss.insert(sys2);
	ss.insert(sys3);

}

#include <cstdint>
#include <ecs/ecs.h>

// An example structured like the EnTT example from
// https://github.com/skypjack/entt#code-example
// to compare interfaces

struct position {
	float x;
	float y;
};

struct velocity {
	float dx;
	float dy;
};

int main()
{
	float constexpr dt = 1.0f/60;

	auto &zero_vel = ecs::add_system([](velocity &vel) {
		vel.dx = 0.;
		vel.dy = 0.;
	});

	auto &update_pos = ecs::add_system([&dt](position &pos, velocity const& vel) {
		pos.x += vel.dx * dt;
		pos.y += vel.dy * dt;
	});

	ecs::entity_range{ 0, 9, [](ecs::entity_id ent) { return position{ ent.id * 1.f, ent.id * 1.f }; } };
	ecs::entity_range{ 0, 4, [](ecs::entity_id ent) { return velocity{ ent.id * 1.f, ent.id * 1.f }; } };
	ecs::commit_changes();

	update_pos.update();
	zero_vel.update();
}

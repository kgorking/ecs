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

struct frame {
	ecs_flags(ecs::share, ecs::immutable); // kind of like a 'static const' member function
	float dt;
};

auto& zero_vel = ecs::make_system([](velocity& vel) {
	vel.dx = 0.;
	vel.dy = 0.;
});

auto& update_pos = ecs::make_system([](position& pos, velocity const& vel, frame const& frame) {
	pos.x += vel.dx * frame.dt;
	pos.y += vel.dy * frame.dt;
});

int main() {
	// Set up the components
	ecs::entity_range{ 0, 9, [](ecs::entity_id ent) { return position{ ent.id * 1.f, ent.id * 1.f }; }, frame{} };
	ecs::entity_range{ 0, 4, [](ecs::entity_id ent) { return velocity{ ent.id * 1.f, ent.id * 1.f }; } };
	ecs::commit_changes();

	// Set the 'frame' delta time
	ecs::get_shared_component<frame>().dt = 1.0f / 60.f;

	// Run the systems
	update_pos.update();
	zero_vel.update();
}

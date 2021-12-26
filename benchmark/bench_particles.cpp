#include <complex>
#include <ecs/ecs.h>

#include "global.h"
#include "gbench/include/benchmark/benchmark.h"

// https://docs.unrealengine.com/en-US/Resources/ContentExamples/EffectsGallery/1_D/index.html
// https://docs.unrealengine.com/en-US/Resources/ContentExamples/EffectsGallery/2_A/index.html
// https://docs.unrealengine.com/en-US/Resources/ContentExamples/EffectsGallery/2_D/index.html
// https://docs.unrealengine.com/en-US/Resources/ContentExamples/EffectsGallery/2_E/index.html

constexpr float delta_time = 1.0f / 60.0f;

using namespace ecs::flag;

struct particle { float x, y; };
struct color    { float r, g, b; };
struct velocity { float x, y; };
struct life     { float val; };
struct dead_tag { ecs_flags(tag, transient); };
struct gravity  { ecs_flags(global); float g = 0.2f; };

// Helper lambda to initialize a particle
auto constexpr particle_init = []() -> particle {
	float const x = rand() / 16384.0f - 1.0f;
	float const y = rand() / 16384.0f - 1.0f;

	return {x, y};
};

// Helper lambda to initialize a color
auto constexpr color_init = []() -> color {
	auto const p = particle_init();
	float const r = p.x / 2 + 0.5f;
	float const g = p.y / 2 + 0.5f;

	return {r, g, 1 - r - g};
};

// Helper-lambda to init the velocity
auto constexpr velocity_init = []() -> velocity {
	auto const p = particle_init();
    float const len = sqrt(p.x * p.x + p.y * p.y) * 10;
    return { p.x / len, p.y / len };
};

// Helper-lambda to init the life
auto constexpr life_init = []() -> life {
	float const x = rand() / 327680.0f; // [0, 0.1]
	return {0.5f + x*10}; // [0.5, 1.5]
};

void make_systems(ecs::runtime &ecs) {
    // Apply gravity to the velocity
    ecs.make_system([](velocity& vel, gravity const& grav) { vel.y -= grav.g * delta_time; });

    // Update a particles position from its velocity
    ecs.make_system([](particle& par, velocity const& vel) {
        par.x += vel.x * delta_time;
        par.y += vel.y * delta_time;
    });

    // Make sure the particles stay within the bounds.
    ecs.make_system([](particle& par, velocity& vel) {
        if (par.x > 1) {
            par.x = 1;
            float const p = 2 * vel.x * -1;
            vel.x = vel.x - p * -1;
        } else if (par.x < -1) {
            par.x = -1;
            float const p = 2 * vel.x * 1;
            vel.x = vel.x - p * 1;
        }

        if (par.y > 1) {
            par.y = 1;
            float const p = 2 * vel.y * -1;
            vel.y = vel.y - p * -1;
        } else if (par.y < -1) {
            par.y = -1;
            float const p = 2 * vel.y * 1;
            vel.y = vel.y - p * 1;
        }
    });

    // Paint particles purple if they are in range of (0.0, 0.0)
    ecs.make_system([](color& col, particle const& par) {
        float const len_sqr = par.x * par.x + par.y * par.y;

        if (len_sqr > 0.0005f)
            return; // out of range

        col.r = 1;
        col.g = 0;
        col.b = 1;
    });

    // Decrease life of live particles
    ecs.make_system([&ecs](ecs::entity_id ent, life& l, dead_tag*) {
        l.val -= delta_time;
        if (l.val < 0) {
            ecs.add_component(ent, dead_tag{});
        }
    });

    // Necromance dead particles
    ecs.make_system([](dead_tag, particle& par, velocity& vel, color& col, life& l) {
        par = particle_init();
        vel = velocity_init();
        col = color_init();
        l = life_init();
    });
}

void particles(benchmark::State& state) {
	auto const num_particles = static_cast<ecs::detail::entity_type>(state.range(0));

	std::vector<particle> particles(num_particles + 1);
	std::vector<velocity> velocities(num_particles + 1);
	std::vector<color> colors(num_particles + 1);
	std::vector<life> lifes(num_particles + 1);

	std::ranges::generate(particles, particle_init);
	std::ranges::generate(velocities, velocity_init);
	std::ranges::generate(colors, color_init);
	std::ranges::generate(lifes, life_init);

    for ([[maybe_unused]] auto const _ : state) {
        ecs::runtime ecs;
        make_systems(ecs);
		ecs.add_component_span({0, num_particles}, particles);
		ecs.add_component_span({0, num_particles}, velocities);
		ecs.add_component_span({0, num_particles}, colors);
		ecs.add_component_span({0, num_particles}, lifes);
        ecs.commit_changes();

		ecs.update();
	}

	state.SetItemsProcessed(state.iterations() * num_particles);
}
ECS_BENCHMARK(particles);

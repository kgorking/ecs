#include <ecs/ecs.h>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

struct health {
	int hp = 100;
};

struct infection {
	static constexpr int dmg = 8;		   // dmg per tick
	static constexpr int dmg_tick = 2;	   // how often it damages
	static constexpr int spread_tick = 1;  // how often it spreads
	static constexpr int spread_range = 2; // how far it spreads

	int duration = 6000; // lasts for 6 seconds
};

int main() {
	ecs::runtime rt;

	// Handle damage logic
	rt.make_system<ecs::opts::interval<1000 / infection::dmg_tick>>([](ecs::entity_id self, health &h, infection const &) {
		// Subtract the damage from the health component
		h.hp -= infection::dmg;

		std::cout << "entity " << self << " took " << infection::dmg << " damage, health is now " << h.hp << '\n';
	});

	// Handle spread logic
	rt.make_system<ecs::opts::interval<1000 / infection::spread_tick>>([&rt](ecs::entity_id self, infection const &p) {
		// Do a spread tick. Use hardcoded entities for simplicitys sake
		auto const ents_in_range = {
			ecs::entity_id{1}, ecs::entity_id{2}}; /* should find all entities (with health component) in spread_range using game logic */
		for (auto const ent : ents_in_range) {
			if (!rt.has_component<infection>(ent)) {
				if (rt.get_component<health>(ent)->hp > 0) {
					// Add a copy of the infection component if the entity doesn't already have it.
					// This means that newly infected entities are only affected for
					// the remaing duration of this infection component
					rt.add_component(ent, infection{p}); // entity 1 and 2 survives
					// ecs::add_component(ent, infection{});   // start a fresh infection instead. Entity 1 dies as well

					std::cout << "entity " << self << " infected entity " << ent << '\n';
				}
			}
		}
	});

	// Handle spell logic
	rt.make_system<ecs::opts::interval<1000 / 10>>([&rt](ecs::entity_id self, infection &p, health const &h) {
		p.duration -= 100;
		bool remove_spell = false;

		if (h.hp <= 0) {
			// The plague did its job, so remove it from the entity
			remove_spell = true;

			std::cout << "entity " << self << " has died of the plague.\n";
		} else if (p.duration <= 0 && h.hp > 0) {
			// The spell has run its course without depleting the health, so remove it.
			remove_spell = true;

			std::cout << "entity " << self << " is no longer infected\n";
		}

		if (remove_spell)
			rt.remove_component(self, p);
	});

	// Add health components to entities 0, 1, 2
	rt.add_component(0, health{80});
	rt.add_component(1, health{100});
	rt.add_component(2, health{120});

	// Infect the first entity
	rt.add_component(0, infection{});

	// Simulate a game loop. Keep going until the plague is gone x_x
	do {
		// Commits changes to components and runs the system
		rt.update();

		std::this_thread::sleep_for(10ms);

	} while (rt.get_component_count<infection>() > 0);
}

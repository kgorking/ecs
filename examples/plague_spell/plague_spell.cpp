#include <ecs/ecs.h>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

constexpr int delta_time = 10; // tick rate

struct health {
    int hp = 30;
};

struct plague {
    static constexpr int dmg = 3;            // dmg per tick
    static constexpr int dmg_tick = 500;     // how often it damages
    static constexpr int spread_tick = 1500; // how often it spreads
    static constexpr int spread_range = 2;   // how far it spreads

    int duration = 6000; // lasts for 6 seconds
    int dmg_duration = dmg_tick;
    int spread_duration = spread_tick;
};

// Handle damage logic
void do_damage_logic(ecs::entity_id self, plague& p, health& h) {
    p.dmg_duration -= delta_time;
    if (p.dmg_duration <= 0) {
        // Reset the tick duration
        p.dmg_duration += plague::dmg_tick;

        // Subtract the damage from the health component
        h.hp -= plague::dmg;

        if (h.hp <= 0) {
            // The plague did its job, so remove it from the entity
            ecs::remove_component(self, p);

            std::cout << "entity " << self << " has died of the plague.\n";
        } else
            std::cout << "entity " << self << " took " << plague::dmg << " damage, health is now " << h.hp
                      << '\n';
    }
}

// Handle spread logic
void do_spread_logic(ecs::entity_id self, plague& p) {
    p.spread_duration -= delta_time;
    if (p.spread_duration <= 0) {
        // Reset the tick duration
        p.spread_duration += plague::spread_tick;

        // Do a spread tick. Use hardcoded entities for simplicitys sake
        auto ents_in_range = {
            ecs::entity_id{1},
            ecs::entity_id{2}}; /* should find all entities (with health component) in spread_range using game logic */
        for (auto ent : ents_in_range) {
            if (!ecs::has_component<plague>(ent)) {
                // Add a copy of the plague component if the entity doesn't already have it.
                // This means that newly infected entities are only affected for
                // the remaing duration of this plague component
                ecs::add_component(ent, plague{p});     // entity 1 and 2 survives (barely)
                // ecs::add_component(ent, plague{});   // start a fresh plague instead. Entity 1 and 2 dies as well

                std::cout << "entity " << self << " infected entity " << ent << '\n';
            }
        }
    }
}

// Handle spell logic
void do_spell_logic(ecs::entity_id self, plague& p, health const& h) {
    p.duration -= delta_time;
    if (p.duration <= 0 && h.hp > 0) {
        // The spell has run its course without depleting the health, so remove it.
        ecs::remove_component(self, p);

        std::cout << "entity " << self << " is no longer infected\n";
    }
}

// The plague spell lambda that works on entities with a plague- and health component
auto constexpr plague_spell_lambda = [](ecs::entity_id self, plague& p, health& h) {
    do_damage_logic(self, p, h);
    do_spread_logic(self, p);
    do_spell_logic(self, p, h);
};

int main() {
    // Create the plague spell system from the lambda
    ecs::make_system(plague_spell_lambda);

    // Add health components to entities 0, 1, 2
    ecs::add_component({0, 2}, health{});

    // Infect the first entity
    ecs::add_component(0, plague{});

    // Simulate a game loop. Keep going until the plague is gone x_x
    do {
        // Commits changes to components and runs the system
        ecs::update_systems();

        std::this_thread::sleep_for(delta_time * 1ms);

    } while (ecs::get_component_count<plague>() > 0);
}

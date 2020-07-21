#include <ecs/ecs.h>
#include <iostream>

// This is a slightly altered version of the 'shared_components' example
// where the state_s component is never added to any entities, but is
// still accessible from the system

struct A {};
struct B {};
struct state_s {
    ecs_flags(ecs::global);
    int a = 0;
    int b = 0;
    int total = 0;
};

static void print_global_state() {
    auto const& global = ecs::get_global_component<state_s>();
    std::cout << "  A touches:       " << global.a << "\n";
    std::cout << "  B touches:       " << global.b << "\n";
    std::cout << "  state_s touches: " << global.total << "\n\n";
}

int main() {
    std::cout << "Initial state:\n";
    print_global_state();

    ecs::make_system([](A const&, state_s& state) {
        state.a++;
        state.total++;
    });
    ecs::make_system([](B const&, state_s& state) {
        state.b++;
        state.total++;
    });

    std::cout << "Adding 10 entities with an A component:\n";
    ecs::add_component({0, 9}, A{});
    
    std::cout << "Adding 10 more entities with a B component:\n";
    ecs::add_component({10, 19}, B{});

    ecs::update_systems();
    print_global_state();

    // Dump some stats
    std::cout << "Number of entities with an A component:      " << ecs::get_entity_count<A>() << "\n";
    std::cout << "Number of entities with a B component:       " << ecs::get_entity_count<B>() << "\n";
    std::cout << "Number of entities with a state_s component: " << ecs::get_entity_count<state_s>() << "\n";
    std::cout << "Number of A components allocated:            " << ecs::get_component_count<A>() << "\n";
    std::cout << "Number of B components allocated:            " << ecs::get_component_count<B>() << "\n";
    std::cout << "Number of state_s components allocated:      " << ecs::get_component_count<state_s>() << "\n";
}

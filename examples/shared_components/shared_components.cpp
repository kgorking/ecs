#include <ecs/ecs.h>
#include <iostream>


struct A {};
struct B {};
struct state_s {
    ecs_flags(ecs::flag::share);
    int a = 0;
    int b = 0;
    int total = 0;
};

static void print_shared_state() {
    auto const& shared = ecs::get_shared_component<state_s>();
    std::cout << " A touches:       " << shared.a << "\n";
    std::cout << " B touches:       " << shared.b << "\n";
    std::cout << " state_s touches: " << shared.total << "\n\n";
}

int main() {
    std::cout << "Initial state:\n";
    print_shared_state();

    auto& sys_a = ecs::make_system([](A const&, state_s& state) {
        state.a++;
        state.total++;
    });
    auto& sys_b = ecs::make_system([](B const&, state_s& state) {
        state.b++;
        state.total++;
    });

    std::cout << "Adding 10 entities with an A and state_s component:\n";
    ecs::add_component({0, 9}, A{}, state_s{});
    ecs::commit_changes();
    sys_a.run(); // run A system
    print_shared_state();

    std::cout << "Adding 10 more entities with a B and state_s component:\n";
    ecs::add_component({10, 19}, B{}, state_s{});
    ecs::commit_changes();
    sys_b.run(); // run B system
    print_shared_state();

    // Dump some stats
    std::cout << "Number of entities with an A component:      " << ecs::get_entity_count<A>() << "\n";
    std::cout << "Number of entities with a B component:       " << ecs::get_entity_count<B>() << "\n";
    std::cout << "Number of entities with a state_s component: " << ecs::get_entity_count<state_s>() << "\n";
    std::cout << "Number of A components allocated:            " << ecs::get_component_count<A>() << "\n";
    std::cout << "Number of B components allocated:            " << ecs::get_component_count<B>() << "\n";
    std::cout << "Number of state_s components allocated:      " << ecs::get_component_count<state_s>() << "\n";
}

#include <ecs/ecs.h>
#include <iostream>


// Inspired by https://www.youtube.com/watch?v=gKbORJtnVu8
// Open to new alternatives (states/events), Open to new operations (systems)

// States
struct state_idle {};
struct state_connected {};
struct state_connecting {
    static constexpr int max_n = 5;
    int n = 0;
};

// Events. Marked as transient so they are automatically removed
struct ev_connect_t {
    ecs_flags(ecs::transient);
};
struct ev_timeout_t {
    ecs_flags(ecs::transient);
};
struct ev_connected_t {
    ecs_flags(ecs::transient);
};
struct ev_disconnect_t {
    ecs_flags(ecs::transient);
};

// Add the systems that handle state/event interactions
void add_systems() {

    // state_idle + ev_connect_t -> state_connecting (1)
    ecs::make_system([](ecs::entity_id fsm, state_idle const&, ev_connect_t const& /*ev*/) {
        std::cout << "ev_connect_t: state_idle -> state_connecting\n";
        ecs::remove_component<state_idle>(fsm);
        ecs::add_component<state_connecting>(fsm);
    });

    // state_connecting + ev_timeout_t [-> state_idle] (2)
    ecs::make_system([](ecs::entity_id fsm, state_connecting& state, ev_timeout_t const& /*ev*/) {
        std::cout << "ev_timeout_t: ";
        if (++state.n >= state_connecting::max_n) {
            std::cout << "state_connecting -> state_idle\n";
            ecs::remove_component<state_connecting>(fsm);
            ecs::add_component<state_idle>(fsm);
        } else {
            std::cout << "n = " << state.n << ", retrying\n";
        }
    });

    // state_connecting + ev_connected_t -> state_connected (3)
    ecs::make_system([](ecs::entity_id fsm, state_connecting const&, ev_connected_t const& /*ev*/) {
        std::cout << "ev_connected_t: state_connecting -> state_connected\n";
        ecs::remove_component<state_connecting>(fsm);
        ecs::add_component<state_connected>(fsm);
    });

    // state_connected + ev_disconnect_t -> state_idle (4)
    ecs::make_system([](ecs::entity_id fsm, state_connected&, ev_disconnect_t const& /*ev*/) {
        std::cout << "ev_disconnect_t: state_connected -> state_idle\n";
        ecs::remove_component<state_connected>(fsm);
        ecs::add_component<state_idle>(fsm);
    });
}

int main() {
    // Add the systems to handle the events
    add_systems();

    // Create the finite state machine entity with the initial state of idle
    ecs::entity_id const fsm{0};
    ecs::add_component<state_idle>(fsm);
    ecs::commit_changes();

    // Add a 'connect' event to the fsm, and commit and run any appropiate systems.
    // Will trigger the 'state_idle/ev_connect_t' system (1) and change the state to
    // 'state_connecting'
    ecs::add_component<ev_connect_t>(fsm);
    ecs::update_systems();

    // Add a 'timeout' event to the fsm, and commit and run any appropiate systems.
    // Will trigger the 'state_connecting/ev_timeout_t' system (2). If too many timeouts happen,
    // change the state back to idle
    ecs::add_component<ev_timeout_t>(fsm);
    ecs::update_systems();

    // Add a 'connected' event to the fsm, and commit and run any appropiate systems.
    // Will trigger the 'state_connecting/ev_connected_t' system (3) and change the state to
    // 'state_connected'
    ecs::add_component<ev_connected_t>(fsm);
    ecs::update_systems();

    // Add a 'disconnect' event to the fsm, and commit and run any appropiate systems.
    // Will trigger the 'state_connected/ev_disconnect_t' system (4) and change the state to
    // 'state_idle'
    ecs::add_component<ev_disconnect_t>(fsm);
    ecs::update_systems();

    // Add a new event and system
    struct ev_hello {
        const char* msg = "hello!";
    };
    ecs::make_system([](state_idle const&, ev_hello const& ev) {
        std::cout << "ev_hello: state_idle says '" << ev.msg << "'\n";
    });

    ecs::add_component<ev_hello>(fsm);
    ecs::update_systems();
}

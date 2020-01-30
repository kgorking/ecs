#include <iostream>
#include <ecs/ecs.h>

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
struct ev_connect_t    { ecs_flags(ecs::transient); };
struct ev_timeout_t    { ecs_flags(ecs::transient); };
struct ev_connected_t  { ecs_flags(ecs::transient); };
struct ev_disconnect_t { ecs_flags(ecs::transient); };

// Add the systems that handle state/event interactions
void add_systems() {

	// state_idle + ev_connect_t -> state_connecting (1)
	ecs::add_system([](ecs::entity fsm, state_idle const&, ev_connect_t const& /*ev*/) {
		std::cout << "ev_connect_t: state_idle -> state_connecting\n";
		fsm.remove<state_idle>();
		fsm.add<state_connecting>();
	});

	// state_connecting + ev_timeout_t [-> state_idle] (2)
	ecs::add_system([](ecs::entity fsm, state_connecting& state, ev_timeout_t const& /*ev*/) {
		std::cout << "ev_timeout_t: ";
		if (++state.n >= state_connecting::max_n) {
			std::cout << "state_connecting -> state_idle\n";
			fsm.remove<state_connecting>();
			fsm.add<state_idle>();
		}
		else {
			std::cout << "n = " << state.n << ", retrying\n";
		}
	});

	// state_connecting + ev_connected_t -> state_connected (3)
	ecs::add_system([](ecs::entity fsm, state_connecting const&, ev_connected_t const& /*ev*/) {
		std::cout << "ev_connected_t: state_connecting -> state_connected\n";
		fsm.remove<state_connecting>();
		fsm.add<state_connected>();
	});

	// state_connected + ev_disconnect_t -> state_idle (4)
	ecs::add_system([](ecs::entity fsm, state_connected&, ev_disconnect_t const& /*ev*/) {
		std::cout << "ev_disconnect_t: state_connected -> state_idle\n";
		fsm.remove<state_connected>();
		fsm.add<state_idle>();
	});
}

int main() {
	// Add the systems to handle the events
	add_systems();

	// Create the finite state machine entity with the initial state of idle
	ecs::entity fsm{ 0, state_idle{} };
	ecs::commit_changes();

	// Add a 'connect' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_idle/ev_connect_t' system (1) and change the state to 'state_connecting'
	fsm.add<ev_connect_t>();
	ecs::update_systems();

	// Add a 'timeout' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_connecting/ev_timeout_t' system (2). If too many timeouts happen, change the state back to idle
	fsm.add<ev_timeout_t>();
	ecs::update_systems();

	// Add a 'connected' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_connecting/ev_connected_t' system (3) and change the state to 'state_connected'
	fsm.add<ev_connected_t>();
	ecs::update_systems();

	// Add a 'disconnect' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_connected/ev_disconnect_t' system (4) and change the state to 'state_idle'
	fsm.add<ev_disconnect_t>();
	ecs::update_systems();

	// Add a new event and system
	struct ev_hello {
		const char* msg = "hello!";
	};
	ecs::add_system([](ecs::entity /*fsm*/, state_idle const&, ev_hello const& ev) {
		std::cout << "ev_hello: state_idle says '" << ev.msg << "'\n";
	});

	fsm.add<ev_hello>();
	ecs::update_systems();
}

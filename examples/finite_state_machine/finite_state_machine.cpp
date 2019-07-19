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
struct ev_connect    : ecs::transient {};
struct ev_timeout    : ecs::transient {};
struct ev_connected  : ecs::transient {};
struct ev_disconnect : ecs::transient {};

// Add the systems that handle state/event interactions
void add_systems() {
	// state_idle + ev_connect -> state_connecting
	ecs::add_system([](ecs::entity fsm, state_idle const&, ev_connect const& /*ev*/) {
		std::cout << "ev_connect: state_idle -> state_connecting\n";
		fsm.remove<state_idle>();
		fsm.add<state_connecting>();
	});

	// state_connecting + ev_timeout [-> state_idle]
	ecs::add_system([](ecs::entity fsm, state_connecting& state, ev_timeout const& /*ev*/) {
		std::cout << "ev_timeout: ";
		if (++state.n >= state_connecting::max_n) {
			std::cout << "state_connecting -> state_idle\n";
			fsm.remove<state_connecting>();
			fsm.add<state_idle>();
		}
		else {
			std::cout << "n = " << state.n << ", retrying\n";
		}
	});

	// state_connecting + ev_connected -> state_connected
	ecs::add_system([](ecs::entity fsm, state_connecting const&, ev_connected const& /*ev*/) {
		std::cout << "ev_connected: state_connecting -> state_connected\n";
		fsm.remove<state_connecting>();
		fsm.add<state_connected>();
	});

	// state_connected + ev_disconnect -> state_idle
	ecs::add_system([](ecs::entity fsm, state_connected&, ev_disconnect const& /*ev*/) {
		std::cout << "ev_disconnect: state_connected -> state_idle\n";
		fsm.remove<state_connected>();
		fsm.add<state_idle>();
	});
}

int main() {
	// Add the systems to handle the events
	add_systems();

	// Create the finite state machine entity with the initial state of idle
	ecs::entity fsm{ 0, state_idle{} };

	// Commit the changes internally
	ecs::commit_changes();

	// Add a 'connect' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_idle/ev_connect' system and change the state to 'connecting'
	fsm.add<ev_connect>();
	ecs::update_systems();

	// Add a 'timeout' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_connecting/ev_timeout' system. If too many timeouts happen, change the state back to idle
	fsm.add<ev_timeout>();
	ecs::update_systems();

	// Add a 'connected' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_connecting/ev_connected' system and change the state to 'connected'
	fsm.add<ev_connected>();
	ecs::update_systems();

	// Add a 'disconnect' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_connected/ev_disconnect' system and change the state to 'idle'
	fsm.add<ev_disconnect>();
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

#include <iostream>
#include <ecs/ecs.h>

// inspired by https://www.youtube.com/watch?v=gKbORJtnVu8
// Open to new alternatives (states/events), Open to new operations (systems)

// states
struct state_idle {};
struct state_connected {};
struct state_connecting {
	static constexpr int max_n = 5;
	int n = 0;
};

// events
struct ev_connect    : ecs::transient {};
struct ev_timeout    : ecs::transient {};
struct ev_connected  : ecs::transient {};
struct ev_disconnect : ecs::transient {};

int main()
{
	// state_idle -> ev_connect -> state_connecting
	ecs::add_system([](ecs::entity fsm, state_idle const&, ev_connect const& /*ev*/) {
		std::cout << "ev_connect: idle -> connecting\n";
		fsm.remove<state_idle>();
		fsm.add<state_connecting>();
	});

	// state_connecting -> ev_timeout [-> state_idle]
	ecs::add_system([](ecs::entity fsm, state_connecting &state, ev_timeout const& /*ev*/) {
		std::cout << "ev_timeout: ";
		if (++state.n >= state_connecting::max_n) {
			std::cout << "connecting -> idle\n";
			fsm.remove<state_connecting>();
			fsm.add<state_idle>();
		}
		else {
			std::cout << "n = " << state.n << ", retrying\n";
		}
	});

	// state_connecting -> ev_connected -> state_connected
	ecs::add_system([](ecs::entity fsm, state_connecting const&, ev_connected const& /*ev*/) {
		std::cout << "ev_connected: connecting -> connected\n";
		fsm.remove<state_connecting>();
		fsm.add<state_connected>();
	});

	// state_connected -> ev_disconnect -> state_idle
	ecs::add_system([](ecs::entity fsm, state_connected &, ev_disconnect const& /*ev*/) {
		std::cout << "ev_disconnect: connected -> idle\n";
		fsm.remove<state_connected>();
		fsm.add<state_idle>();
	});

	// Initial state
	ecs::entity fsm{ 0 };

	fsm.add<state_idle>();
	ecs::update_systems();

	fsm.add<ev_connect>();
	ecs::update_systems();

	fsm.add<ev_timeout>();
	ecs::update_systems();

	fsm.add<ev_connected>();
	ecs::update_systems();

	fsm.add<ev_disconnect>();
	ecs::update_systems();
}

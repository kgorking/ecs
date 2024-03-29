#include <ecs/ecs.h>
#include <iostream>

// Inspired by https://www.youtube.com/watch?v=gKbORJtnVu8
// Open to new alternatives (states/events), Open to new operations (systems)

// States
struct state_idle {
	using ecs_flags = ecs::flags<ecs::tag>;
};
struct state_connecting {
	using variant_of = state_idle;
	static constexpr int max_n = 5;
	int n = 0;
};
struct state_connected {
	using variant_of = state_connecting;
	using ecs_flags = ecs::flags<ecs::tag>;
};

// Events. Marked as transient so they are automatically removed
struct ev_connect_t { using ecs_flags = ecs::flags<ecs::transient>; };
struct ev_timeout_t { using ecs_flags = ecs::flags<ecs::transient>; };
struct ev_connected_t { using ecs_flags = ecs::flags<ecs::transient>; };
struct ev_disconnect_t { using ecs_flags = ecs::flags<ecs::transient>; };

// Add the systems that handle state/event interactions
static void add_systems(ecs::runtime& rt) {

	// state_idle + ev_connect_t -> state_connecting (1)
	rt.make_system([&](ecs::entity_id fsm, state_idle, ev_connect_t const& /*ev*/) {
		std::cout << "ev_connect_t: state_idle -> state_connecting\n";
		rt.add_component(fsm, state_connecting{});
	});

	// state_connecting + ev_timeout_t [-> state_idle] (2)
	rt.make_system([&](ecs::entity_id fsm, state_connecting& connecting, ev_timeout_t const& /*ev*/) {
		std::cout << "ev_timeout_t: ";
		if (++connecting.n >= state_connecting::max_n) {
			std::cout << "state_connecting -> state_idle\n";
			rt.add_component(fsm, state_idle{});
		} else {
			std::cout << "n = " << connecting.n << ", retrying\n";
		}
	});

	// state_connecting + ev_connected_t -> state_connected (3)
	rt.make_system([&](ecs::entity_id fsm, state_connecting, ev_connected_t const& /*ev*/) {
		std::cout << "ev_connected_t: state_connecting -> state_connected\n";
		rt.add_component(fsm, state_connected{});
	});

	// state_connected + ev_disconnect_t -> state_idle (4)
	rt.make_system([&](ecs::entity_id fsm, state_connected, ev_disconnect_t const& /*ev*/) {
		std::cout << "ev_disconnect_t: state_connected -> state_idle\n";
		rt.add_component(fsm, state_idle{});
	});
}

int main() {
	ecs::runtime rt;

	// Add the systems to handle the events
	add_systems(rt);

	// Create the finite state machine entity with the initial state of idle
	ecs::entity_id const fsm{0};
	rt.add_component(fsm, state_idle{});
	rt.commit_changes();

	// Add a 'connect' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_idle/ev_connect_t' system (1) and change the state to
	// 'state_connecting'
	rt.add_component(fsm, ev_connect_t{});
	rt.update();

	// Add a 'timeout' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_connecting/ev_timeout_t' system (2). If too many timeouts happen,
	// change the state back to idle
	rt.add_component(fsm, ev_timeout_t{});
	rt.update();

	// Add a 'connected' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_connecting/ev_connected_t' system (3) and change the state to
	// 'state_connected'
	rt.add_component(fsm, ev_connected_t{});
	rt.update();

	// Add a 'disconnect' event to the fsm, and commit and run any appropiate systems.
	// Will trigger the 'state_connected/ev_disconnect_t' system (4) and change the state to
	// 'state_idle'
	rt.add_component(fsm, ev_disconnect_t{});
	rt.update();

	// Add a new event and system
	struct ev_hello {
		using ecs_flags = ecs::flags<ecs::transient>;
		const char* msg = "hello!";
	};
	rt.make_system([](state_idle, ev_hello const &ev) { std::cout << "ev_hello: state_idle says '" << ev.msg << "'\n"; });

	rt.add_component(fsm, ev_hello{});
	rt.update();
}

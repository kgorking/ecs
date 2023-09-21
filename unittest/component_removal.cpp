#include <ecs/ecs.h>
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

//
// Test to make sure components are properly removed from systems.
// This is based on a bug that was exposed by the 'finite_state_machine' example.
TEST_CASE("Component removal", "[component][transient]") {
	ecs::runtime ecs;

	struct state_idle {};
	struct state_connecting {};
	struct ev_connect {
		using ecs_flags = ecs::flags<ecs::transient>;
	};
	struct ev_timeout {
		using ecs_flags = ecs::flags<ecs::transient>;
	};

	int run_counter_idle = 0;
	ecs.make_system<ecs::opts::not_parallel>([&](state_idle const& /*idle*/, ev_connect const& /*ev*/) { run_counter_idle++; });

	ecs::entity_id const fsm{0};
	ecs.add_component(fsm, state_idle{});

	ecs.commit_changes();

	ecs.add_component(fsm, ev_connect{});
	ecs.update();
	CHECK(run_counter_idle == 1);

	ecs.add_component(fsm, ev_timeout{});
	ecs.update();
	CHECK(run_counter_idle == 1);
}

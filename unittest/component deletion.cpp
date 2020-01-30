#include <ecs/ecs.h>
#include "catch.hpp"

//
// Test to make sure components are properly removed from systems.
// This is based on a bug that was exposed by the 'finite_state_machine' example.
TEST_CASE("Component removal", "[component][transient]")
{
	ecs::detail::_context.reset();

	struct state_idle {};
	struct state_connecting {};
	struct ev_connect { ecs_flags(ecs::transient); };
	struct ev_timeout { ecs_flags(ecs::transient); };

	int run_counter_idle = 0;
	ecs::detail::_context.init_component_pools<ev_timeout>();
	ecs::add_system([&](state_idle const&, ev_connect const& /*ev*/) { run_counter_idle++; });

	ecs::entity fsm{ 0, state_idle{} };
	ecs::commit_changes();

	fsm.add<ev_connect>();
	ecs::update_systems();
	CHECK(run_counter_idle == 1);

	fsm.add<ev_timeout>();
	ecs::update_systems();
	CHECK(run_counter_idle == 1);
}

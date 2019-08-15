#include <ecs/ecs.h>
#include "catch.hpp"

//
// Test to make sure components are properly removed from systems.
// This is based on a bug that was exposed by the 'finite_state_machine' example.
TEST_CASE("Component removal", "[component][transient]")
{
	ecs::runtime::reset();

	struct state_idle {};
	struct state_connecting {};
	struct ev_connect : ecs::transient {};
	struct ev_timeout : ecs::transient {};

	int run_counter_idle = 0;
	ecs::runtime::init_components<ev_timeout>();
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

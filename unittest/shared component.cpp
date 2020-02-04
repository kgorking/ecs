#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("Shared components", "[component][shared]")
{
	struct test_s { 
		ecs_flags(ecs::shared);
		int i = 0;
	};

	ecs::detail::_context.reset();

	auto& pst = ecs::get_shared_component<test_s>();
	pst.i = 42;

	ecs::add_system([](test_s const& st) {
		CHECK(42 == st.i);
	});

	ecs::add_component({ 0, 2 }, test_s{});
	ecs::commit_changes();

	// Only 1 test_s should exist
	CHECK(1 == ecs::get_component_count<test_s>());

	// Ensure that different entities have the same shared component
	ptrdiff_t const diff = ecs::get_component<test_s>(0) - ecs::get_component<test_s>(1);
	CHECK(diff == 0);

	// Test the content of the entities
	ecs::run_systems();
}

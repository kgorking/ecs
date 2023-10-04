#include <ecs/ecs.h>
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

// Create a variant tree
//   A
//  / \
// B   C
// |
// D
//
struct A {};
struct B { using variant_of = A; };
struct C { using variant_of = A; };
struct D { using variant_of = B; };

TEST_CASE("Variant components", "[component][variant]") {
	ecs::runtime rt;

	// First, add 'A'
	rt.add_component(0, A{});
	rt.commit_changes();
	REQUIRE(rt.get_component_count<A>() == 1);
	REQUIRE(rt.get_component_count<B>() == 0);
	REQUIRE(rt.get_component_count<C>() == 0);
	REQUIRE(rt.get_component_count<D>() == 0);

	// Add 'B' and 'C'. Both are variants of 'A', so 'A' will be removed
	rt.add_component(0, B{}, C{});
	rt.commit_changes();
	REQUIRE(rt.get_component_count<A>() == 0);
	REQUIRE(rt.get_component_count<B>() == 1);
	REQUIRE(rt.get_component_count<C>() == 1);
	REQUIRE(rt.get_component_count<D>() == 0);

	// Add 'D', which is only a variant of 'B' and 'A', so 'B' will be removed.
	// 'C' remains untouched
	rt.add_component(0, D{});
	rt.commit_changes();
	REQUIRE(rt.get_component_count<A>() == 0);
	REQUIRE(rt.get_component_count<B>() == 0);
	REQUIRE(rt.get_component_count<C>() == 1);
	REQUIRE(rt.get_component_count<D>() == 1);

	// Add 'A', which a parent variant to all other components,
	// so they will all be removed
	rt.add_component(0, A{});
	rt.commit_changes();
	REQUIRE(rt.get_component_count<A>() == 1);
	REQUIRE(rt.get_component_count<B>() == 0);
	REQUIRE(rt.get_component_count<C>() == 0);
	REQUIRE(rt.get_component_count<D>() == 0);

	// Add 'D', which is a variant of 'A'.
	rt.add_component(0, D{});
	rt.commit_changes();
	REQUIRE(rt.get_component_count<A>() == 0);
	REQUIRE(rt.get_component_count<B>() == 0);
	REQUIRE(rt.get_component_count<C>() == 0);
	REQUIRE(rt.get_component_count<D>() == 1);

	// Add 'A' again
	rt.add_component(0, A{});
	rt.commit_changes();
	REQUIRE(rt.get_component_count<A>() == 1);
	REQUIRE(rt.get_component_count<B>() == 0);
	REQUIRE(rt.get_component_count<C>() == 0);
	REQUIRE(rt.get_component_count<D>() == 0);
}

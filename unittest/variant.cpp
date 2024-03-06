#include <ecs/ecs.h>
#include <catch2/catch_test_macros.hpp>
#include "override_contract_handler_to_throw.h"

// A-B-C
struct A {};
struct B { using variant_of = A; };
struct C { using variant_of = B; };

// 
// E-F-H
// E-G
struct E {};
struct F { using variant_of = E; };
struct G { using variant_of = E; };
struct H { using variant_of = F; };

TEST_CASE("Variant components", "[component][variant]") {
	SECTION("list-variant") {
		// Compile-time checks
		#if !defined(ECS_USE_MODULES)
		static_assert(!ecs::detail::has_variant_alias<A>);
		static_assert(ecs::detail::has_variant_alias<B>);
		static_assert(ecs::detail::has_variant_alias<C>);
		static_assert(ecs::detail::is_variant_of<A, B>());
		static_assert(ecs::detail::is_variant_of<A, C>());
		static_assert(ecs::detail::is_variant_of<B, A>());
		static_assert(ecs::detail::is_variant_of<B, C>());
		static_assert(ecs::detail::is_variant_of<C, A>());
		static_assert(ecs::detail::is_variant_of<C, B>());
		#endif

		ecs::runtime rt;

		// Add 'A'
		rt.add_component(0, A{});
		rt.commit_changes();
		REQUIRE(rt.get_component_count<A>() == 1);
		REQUIRE(rt.get_component_count<B>() == 0);
		REQUIRE(rt.get_component_count<C>() == 0);

		// Add 'B', 'A' will be removed
		rt.add_component(0, B{});
		rt.commit_changes();
		REQUIRE(rt.get_component_count<A>() == 0);
		REQUIRE(rt.get_component_count<B>() == 1);
		REQUIRE(rt.get_component_count<C>() == 0);

		// Add 'C', 'B' will be removed
		rt.add_component(0, C{});
		rt.commit_changes();
		REQUIRE(rt.get_component_count<A>() == 0);
		REQUIRE(rt.get_component_count<B>() == 0);
		REQUIRE(rt.get_component_count<C>() == 1);
	}

	SECTION("tree-variant") {
		// Create a variant tree
		//
		ecs::runtime rt;

		// First, add 'E'
		rt.add_component(0, E{});
		rt.commit_changes();
		REQUIRE(rt.get_component_count<E>() == 1);
		REQUIRE(rt.get_component_count<F>() == 0);
		REQUIRE(rt.get_component_count<G>() == 0);
		REQUIRE(rt.get_component_count<H>() == 0);

		// Add 'F' and 'G'. Both are variants of 'E', so 'E' will be removed
		rt.add_component(0, F{}, G{});
		rt.commit_changes();
		REQUIRE(rt.get_component_count<E>() == 0);
		REQUIRE(rt.get_component_count<F>() == 1);
		REQUIRE(rt.get_component_count<G>() == 1);
		REQUIRE(rt.get_component_count<H>() == 0);

		// Add 'H', which is only a variant of 'F' and 'E', so 'F' will be removed.
		// 'G' remains untouched
		rt.add_component(0, H{});
		rt.commit_changes();
		REQUIRE(rt.get_component_count<E>() == 0);
		REQUIRE(rt.get_component_count<F>() == 0);
		REQUIRE(rt.get_component_count<G>() == 1);
		REQUIRE(rt.get_component_count<H>() == 1);

		// Add 'E', which a parent variant to all other components,
		// so they will all be removed
		rt.add_component(0, E{});
		rt.commit_changes();
		REQUIRE(rt.get_component_count<E>() == 1);
		REQUIRE(rt.get_component_count<F>() == 0);
		REQUIRE(rt.get_component_count<G>() == 0);
		REQUIRE(rt.get_component_count<H>() == 0);

		// Add 'H', which is a variant of 'E'.
		rt.add_component(0, H{});
		rt.commit_changes();
		REQUIRE(rt.get_component_count<E>() == 0);
		REQUIRE(rt.get_component_count<F>() == 0);
		REQUIRE(rt.get_component_count<G>() == 0);
		REQUIRE(rt.get_component_count<H>() == 1);

		// Add 'E' again
		rt.add_component(0, E{});
		rt.commit_changes();
		REQUIRE(rt.get_component_count<E>() == 1);
		REQUIRE(rt.get_component_count<F>() == 0);
		REQUIRE(rt.get_component_count<G>() == 0);
		REQUIRE(rt.get_component_count<H>() == 0);
	}

	/*SECTION("Can not add more than one variant at the time") {
		ecs::runtime rt;
		rt.add_component(0, A{});
		rt.add_component(0, B{});

		// This results in a terminate()
		// due to a throw inside a parallel std::for_each, which is `noexcept`
		REQUIRE_THROWS(rt.commit_changes());
	}*/
}

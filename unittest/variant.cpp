#include <ecs/ecs.h>
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "override_contract_handler_to_throw.h"

TEST_CASE("Variant components", "[component][variant]") {
	SECTION("list-variant") {
		// A-B-C
		struct A {};
		struct B { using variant_of = A; };
		struct C { using variant_of = B; };

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

	SECTION("circular-variant") {
		// A-B-C-A
		struct C;
		struct A { using variant_of = C; };
		struct B { using variant_of = A; };
		struct C { using variant_of = B; };

		#if !defined(ECS_USE_MODULES)
		static_assert(!ecs::detail::not_recursive_variant<B>());
		#endif

		// Will not compile
		//ecs::runtime rt;
		//rt.add_component(0, A{});
	}

	SECTION("Can not add more than one variant at the time") {
		struct A {};
		struct B { using variant_of = A; };

		ecs::runtime rt;

		rt.add_component(0, A{});
		rt.add_component(0, B{});

		// This results in a terminate()
		// due to a throw inside a parallel std::for_each, which is `noexcept`
		//REQUIRE_THROWS(rt.commit_changes());
	}
}

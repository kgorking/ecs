#include <ecs/ecs.h>
#include "catch.hpp"
#include <vector>

// A bunch of tests to test constexpr-ness
TEST_CASE("constexpr", "[constexpr]") {
	SECTION("new") {
		//constexpr int* p = new int;
	}

	SECTION("vector") {
		//constexpr std::vector<int> v;
	}

	SECTION("component pool") {
		//constexpr ecs::detail::component_pool<int> pool;
	}

	SECTION("system") {
		//auto lm = [](int&) {};
		//constexpr ecs::detail::system_impl<0, std::execution::sequenced_policy, decltype(lm), int> sys(lm);
	}

	SECTION("context") {
		//constexpr ecs::detail::context ctx;
	}

	SECTION("entity") {
		constexpr ecs::entity ent{ 0 };
		//constexpr ecs::entity ent{ 1, 3.14f };
	}

	SECTION("entity_range") {
		constexpr ecs::entity_range rng{ 0, 5 };
		constexpr ecs::entity_range::iterator it{ 5 };
		//constexpr ecs::entity_range rng{ 0, 5, 3.14f };
	}
}

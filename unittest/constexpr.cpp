#include <ecs/ecs.h>
#include "catch.hpp"
#include <vector>

// A bunch of tests to test constexpr-ness
// Waiting for compiler support
TEST_CASE("constexpr", "[constexpr]") {
	SECTION("new") {
		/*constexpr*/ int* p = new int;
		delete p;
	}

	SECTION("vector") {
		/*constexpr*/ std::vector<int> v{ 0 };
	}

	SECTION("component pool") {
		/*constexpr*/ ecs::detail::component_pool<int> pool;
	}

	SECTION("system") {
		auto lm = [](int&) {};
		/*constexpr*/ auto& sys = ecs::make_system(lm);
		sys.update();
	}

	SECTION("context") {
		/*constexpr*/ ecs::detail::context ctx;
	}

	SECTION("entity") {
		constexpr ecs::entity ent{ 0 };
		//constexpr ecs::entity ent2{ 1, 3.14f };
	}

	SECTION("entity_range") {
		constexpr ecs::entity_range range1{ 0, 5 };
		constexpr ecs::entity_range range2{ 6, 9 };

		constexpr bool equal = range1 == range2;
		REQUIRE(!equal);

		constexpr bool less = range1 < range2;
		REQUIRE(less);

		constexpr bool mergeable = range1.can_merge(range2);
		REQUIRE(mergeable);

		constexpr auto merged_range = ecs::entity_range::merge(range1, range2);
		REQUIRE(10 == merged_range.count());

		constexpr ecs::entity_range intersect_range = ecs::entity_range::intersect(merged_range, range2);
		REQUIRE(intersect_range == range2);

		constexpr auto removed_range = ecs::entity_range::remove(merged_range, { 3,9 });
		REQUIRE(3 == removed_range.first.count());

		constexpr auto offset = merged_range.offset({ 2 });
		REQUIRE(2 == offset);

		constexpr ecs::entity_range::iterator it{ 5 };

		//constexpr ecs::entity_range range_with_component{ 0, 5, 3.14f };
	}
}

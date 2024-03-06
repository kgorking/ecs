#include <ecs/detail/range_tree.h>
#include <exception>
#include <numeric>
#include <catch2/catch_test_macros.hpp>

// Override the default handler for contract violations.
#include "override_contract_handler_to_throw.h"

// A bunch of tests to ensure that the range_tree behaves as expected
TEST_CASE("range_tree specification") {
	SECTION("A new range_tree is empty") {
		ecs::detail::range_tree tree;
		REQUIRE(tree.size() == 0ull);
	}

	SECTION("inserting ranges works") {
		ecs::detail::range_tree tree;
		tree.insert({1, 3});
		tree.insert({6, 7});
		tree.insert({-10, -6});
		REQUIRE(tree.size() == 3ull);

		SECTION("overlap testing works") {
			REQUIRE(tree.overlaps({-14, -5}) == true);
			REQUIRE(tree.overlaps({4, 5}) == false);
			REQUIRE(tree.overlaps({7, 9}) == true);
		}

		SECTION("tree can be iterated") {
			const std::vector<ecs::entity_range> expected{{-10, -6} ,{1, 3}, {6, 7}};
			std::vector<ecs::entity_range> ranges;
			for(auto range : tree) {
				ranges.push_back(range);
			}

			REQUIRE(expected == ranges);
		}
	}

	SECTION("An empty pool") {
		SECTION("does not throw on bad component access") {
		}
	}
}

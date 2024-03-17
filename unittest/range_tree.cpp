#include <ecs/detail/range_tree.h>
#include <catch2/catch_test_macros.hpp>
#include <ranges>

// Make a vector of n ranges, with some gaps in between
std::vector<ecs::entity_range> make_ranges(int n) {
	std::vector<ecs::entity_range> ranges;
	ranges.reserve(n);
	for (int i = 0, offset=0; i < n; i++) {
		ranges.emplace_back(offset, offset + 3 + (i&1));
		offset = ranges.back().last() + 1;
	}
	return ranges;
}

// A bunch of tests to ensure that the range_tree behaves as expected
TEST_CASE("range_tree specification") {
	auto const random_ranges = make_ranges(40);

	SECTION("A new range_tree is empty") {
		ecs::detail::range_tree tree;
		REQUIRE(tree.size() == 0);
	}

	SECTION("inserting ranges works") {
		SECTION("overlap testing works") {
			ecs::detail::range_tree tree;
			tree.insert({1, 3});
			tree.insert({6, 7});
			tree.insert({-10, -6});
			REQUIRE(tree.size() == 3);

			REQUIRE(tree.overlaps({-14, -5}) == true);
			REQUIRE(tree.overlaps({4, 5}) == false);
			REQUIRE(tree.overlaps({7, 9}) == true);
		}

		SECTION("tree can be iterated") {
			auto const expected = make_ranges(25);
			ecs::detail::range_tree tree;
			for (auto const range : expected | std::views::reverse)
				tree.insert(range);
			
			std::vector<ecs::entity_range> ranges;
			for(auto const range : tree)
				ranges.push_back(range);

			REQUIRE(expected == ranges);
		}
	}

	SECTION("balancing") {
		ecs::detail::range_tree tree;
		for (auto r : random_ranges)
			tree.insert(r);

		CHECK(tree.height() < (int)random_ranges.size());
	}
}

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

	SECTION("insert") {
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

	SECTION("remove") {
		ecs::detail::range_tree tree;

		SECTION("full interval removal") {
			tree.insert({0, 10});
			tree.remove({0, 10});
			REQUIRE(tree.size() == 0);
		}

		SECTION("partial interval removal") {
			tree.insert({0, 10});
			tree.remove({1, 9});
			REQUIRE(tree.size() == 2);
		}

		SECTION("multiple interval removals") {
			tree.insert({0, 2});
			tree.insert({5, 7});
			tree.insert({9, 14});
			tree.remove({-10, 20});
			REQUIRE(tree.size() == 0);
		}

		SECTION("multiple+partial interval removals") {
			tree.insert({-2, 2});
			tree.insert({4, 7});
			tree.insert({19, 24});

			tree.remove({0, 6});
			REQUIRE(tree.size() == 3);

			std::vector<ecs::entity_range> ranges;
			for (auto const range : tree)
				ranges.push_back(range);
			REQUIRE(ranges.size() == (std::size_t)3);
			std::vector<ecs::entity_range> expected{{-2, -1}, {7, 7}, {19, 24}};
			REQUIRE(expected == ranges);

			tree.remove({6, 20});
			REQUIRE(tree.size() == 2);
			ranges.clear();
			for (auto const range : tree)
				ranges.push_back(range);
			expected = {{-2, -1}, {21, 24}};
			REQUIRE(expected == ranges);
		}
	}
}

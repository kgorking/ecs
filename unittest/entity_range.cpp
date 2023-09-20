#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#define ECS_EXPORT
#include <ecs/detail/component_pool.h>
#include <ecs/detail/entity_range.h>

using ecs::detail::iter_pair;
using Iter = typename std::vector<ecs::entity_range>::const_iterator;

std::vector<ecs::entity_range> intersect(std::vector<ecs::entity_range> const vec_a, std::vector<ecs::entity_range> const vec_b) {
	return ecs::detail::intersect_ranges_iter(iter_pair<Iter>{vec_a.begin(), vec_a.end()}, iter_pair<Iter>{vec_b.begin(), vec_b.end()});
}

TEST_CASE("entity_range ", "[entity]") {
	SECTION("iterator overflow test") {
		constexpr auto max = std::numeric_limits<ecs::detail::entity_type>::max();
		ecs::entity_range r{max - 1, max};
		int64_t counter = 0;
		for (auto const ent : r) {

			// end iterator becomes max+1, which is the same as std::numeric_limits<ecs::entity_type>::min()
			(void)ent;
			counter++;
		}
		REQUIRE(counter == 2);
	}

	SECTION("intersection tests") {
		SECTION("Empty range A") {
			/// a:
			/// b:      ---     ---     ---
			std::vector<ecs::entity_range> const vec_a{};
			std::vector<ecs::entity_range> const vec_b{{5, 7}, {13, 15}, {21, 23}};

			auto const result = intersect(vec_a, vec_b);

			CHECK(result.empty());
		}

		SECTION("Empty range B") {
			/// a: *****   *****   *****
			/// b:
			std::vector<ecs::entity_range> const vec_a{{0, 4}, {8, 12}, {16, 20}};
			std::vector<ecs::entity_range> const vec_b{};

			auto const result = intersect(vec_a, vec_b);

			CHECK(result.empty());
		}

		SECTION("No overlaps between ranges") {
			/// a: *****   *****   *****
			/// b:      ---     ---     ---
			std::vector<ecs::entity_range> const vec_a{{0, 4}, {8, 12}, {16, 20}};
			std::vector<ecs::entity_range> const vec_b{{5, 7}, {13, 15}, {21, 23}};

			auto const result = intersect(vec_a, vec_b);

			REQUIRE(result.empty());
		}

		SECTION("Ranges in B are contained in ranges in A") {
			/// a: ***** ***** *****
			/// b:  ---   ---   ---
			std::vector<ecs::entity_range> const vec_a{{0, 4}, {5, 9}, {10, 14}};
			std::vector<ecs::entity_range> const vec_b{{1, 3}, {6, 8}, {11, 13}};

			auto const result = intersect(vec_a, vec_b);

			REQUIRE(size_t{3} == result.size());
			CHECK(ecs::entity_range{1, 3}.equals(result.at(0)));
			CHECK(ecs::entity_range{6, 8}.equals(result.at(1)));
			CHECK(ecs::entity_range{11, 13}.equals(result.at(2)));
		}

		SECTION("Ranges in A are contained in ranges in B") {
			/// a:  ---   ---   ---
			/// b: ***** ***** *****
			std::vector<ecs::entity_range> const vec_a{{1, 3}, {6, 8}, {11, 13}};
			std::vector<ecs::entity_range> const vec_b{{0, 4}, {5, 9}, {10, 14}};

			auto const result = intersect(vec_a, vec_b);

			REQUIRE(size_t{3} == result.size());
			CHECK(ecs::entity_range{1, 3}.equals(result.at(0)));
			CHECK(ecs::entity_range{6, 8}.equals(result.at(1)));
			CHECK(ecs::entity_range{11, 13}.equals(result.at(2)));
		}

		SECTION("Ranges in A overlap ranges in B") {
			/// a: *****  *****  *****
			/// b:     ---    ---    ---
			std::vector<ecs::entity_range> const vec_a{{0, 4}, {7, 11}, {14, 18}};
			std::vector<ecs::entity_range> const vec_b{{4, 6}, {11, 13}, {18, 20}};

			auto const result = intersect(vec_a, vec_b);

			REQUIRE(size_t{3} == result.size());
			CHECK(ecs::entity_range{4, 4} == result.at(0));
			CHECK(ecs::entity_range{11, 11} == result.at(1));
			CHECK(ecs::entity_range{18, 18} == result.at(2));
		}

		SECTION("Ranges in B overlap ranges in A") {
			/// a:     ---    ---    ---
			/// b: *****  *****  *****
			std::vector<ecs::entity_range> const vec_a{{4, 6}, {11, 13}, {18, 20}};
			std::vector<ecs::entity_range> const vec_b{{0, 4}, {7, 11}, {14, 18}};

			auto const result = intersect(vec_a, vec_b);

			REQUIRE(size_t{3} == result.size());
			CHECK(ecs::entity_range{4, 4} == result.at(0));
			CHECK(ecs::entity_range{11, 11} == result.at(1));
			CHECK(ecs::entity_range{18, 18} == result.at(2));
		}

		SECTION("Ranges in A overlap multiple ranges in B") {
			/// a: ********* *********
			/// b:  --- ---   --- ---
			std::vector<ecs::entity_range> const vec_a{{0, 8}, {9, 17}};
			std::vector<ecs::entity_range> const vec_b{{1, 3}, {5, 7}, {10, 12}, {14, 16}};

			auto const result = intersect(vec_a, vec_b);

			REQUIRE(size_t{4} == result.size());
			CHECK(ecs::entity_range{1, 3} == result.at(0));
			CHECK(ecs::entity_range{5, 7} == result.at(1));
			CHECK(ecs::entity_range{10, 12} == result.at(2));
			CHECK(ecs::entity_range{14, 16} == result.at(3));
		}

		SECTION("Ranges in B overlap multiple ranges in A") {
			/// a:  --- ---   --- ---
			/// b: ********* *********
			std::vector<ecs::entity_range> const vec_a{{1, 3}, {5, 7}, {10, 12}, {14, 16}};
			std::vector<ecs::entity_range> const vec_b{{0, 8}, {9, 17}};

			auto const result = intersect(vec_a, vec_b);

			REQUIRE(size_t{4} == result.size());
			CHECK(ecs::entity_range{1, 3} == result.at(0));
			CHECK(ecs::entity_range{5, 7} == result.at(1));
			CHECK(ecs::entity_range{10, 12} == result.at(2));
			CHECK(ecs::entity_range{14, 16} == result.at(3));
		}

		SECTION("One range in B overlaps two ranges in A") {
			/// a: *** ***
			/// b:  -----
			std::vector<ecs::entity_range> const vec_a{{1, 3}, {5, 7}};
			std::vector<ecs::entity_range> const vec_b{{2, 6}};

			auto const result = intersect(vec_a, vec_b);

			REQUIRE(size_t{2} == result.size());
			CHECK(ecs::entity_range{2, 3} == result.at(0));
			CHECK(ecs::entity_range{5, 6} == result.at(1));
		}

		SECTION("One range in A overlaps two ranges in B") {
			/// a:  -----
			/// b: *** ***
			std::vector<ecs::entity_range> const vec_a{{2, 6}};
			std::vector<ecs::entity_range> const vec_b{{1, 3}, {5, 7}};

			auto const result = intersect(vec_a, vec_b);

			REQUIRE(size_t{2} == result.size());
			CHECK(ecs::entity_range{2, 3} == result.at(0));
			CHECK(ecs::entity_range{5, 6} == result.at(1));
		}
	}
	SECTION("intersection difference") {
		SECTION("Empty range A") {
			/// a:
			/// b:      ---     ---     ---
			std::vector<ecs::entity_range> const vec_a{};
			std::vector<ecs::entity_range> const vec_b{{5, 7}, {13, 15}, {21, 23}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			CHECK(result.empty());
		}

		SECTION("Empty range B") {
			/// a: *****   *****   *****
			/// b:
			std::vector<ecs::entity_range> const vec_a{{0, 4}, {8, 12}, {16, 20}};
			std::vector<ecs::entity_range> const vec_b{};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			CHECK(result == vec_a);
		}

		SECTION("No overlaps between ranges") {
			/// a: *****   *****   *****
			/// b:      ---     ---     ---
			std::vector<ecs::entity_range> const vec_a{{0, 4}, {8, 12}, {16, 20}};
			std::vector<ecs::entity_range> const vec_b{{5, 7}, {13, 15}, {21, 23}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(result == vec_a);
		}

		SECTION("Remove 1 entity from front of A") {
			/// a: ----
			/// b: |
			std::vector<ecs::entity_range> const vec_a{{0, 3}};
			std::vector<ecs::entity_range> const vec_b{{0, 0}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{1} == result.size());
			CHECK(ecs::entity_range{1, 3}.equals(result.at(0)));
		}

		SECTION("Remove 1 entity from back of A") {
			/// a: ----
			/// b:    |
			std::vector<ecs::entity_range> const vec_a{{0, 3}};
			std::vector<ecs::entity_range> const vec_b{{3, 3}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{1} == result.size());
			CHECK(ecs::entity_range{0, 2}.equals(result.at(0)));
		}

		SECTION("Remove 3 entities from front of A") {
			/// a: ----
			/// b: |||
			std::vector<ecs::entity_range> const vec_a{{0, 3}};
			std::vector<ecs::entity_range> const vec_b{{0, 0}, {1, 1}, {2, 2}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{1} == result.size());
			CHECK(ecs::entity_range{3, 3}.equals(result.at(0)));
		}

		SECTION("Remove 3 entities from back of A") {
			/// a: ----
			/// b:  |||
			std::vector<ecs::entity_range> const vec_a{{0, 3}};
			std::vector<ecs::entity_range> const vec_b{{1, 1}, {2, 2}, {3, 3}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{1} == result.size());
			CHECK(ecs::entity_range{0, 0}.equals(result.at(0)));
		}

		SECTION("Ranges in B are contained in ranges in A") {
			/// a: ***** ***** *****
			/// b:  ---   ---   ---
			std::vector<ecs::entity_range> const vec_a{{0, 4}, {5, 9}, {10, 14}};
			std::vector<ecs::entity_range> const vec_b{{1, 3}, {6, 8}, {11, 13}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{6} == result.size());
			CHECK(ecs::entity_range{0, 0}.equals(result.at(0)));
			CHECK(ecs::entity_range{4, 4}.equals(result.at(1)));
			CHECK(ecs::entity_range{5, 5}.equals(result.at(2)));
			CHECK(ecs::entity_range{9, 9}.equals(result.at(3)));
			CHECK(ecs::entity_range{10, 10}.equals(result.at(4)));
			CHECK(ecs::entity_range{14, 14}.equals(result.at(5)));
		}

		SECTION("Ranges in A are contained in ranges in B") {
			/// a:  ---   ---   ---
			/// b: ***** ***** *****8
			std::vector<ecs::entity_range> const vec_a{{1, 3}, {6, 8}, {11, 13}};
			std::vector<ecs::entity_range> const vec_b{{0, 4}, {5, 9}, {10, 14}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{0} == result.size());
		}

		SECTION("Ranges in A overlap ranges in B") {
			/// a: *****  *****  *****
			/// b:     ---    ---    ---
			std::vector<ecs::entity_range> const vec_a{{0, 4}, {7, 11}, {14, 18}};
			std::vector<ecs::entity_range> const vec_b{{4, 6}, {11, 13}, {18, 20}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{3} == result.size());
			CHECK(ecs::entity_range{0, 3} == result.at(0));
			CHECK(ecs::entity_range{7, 10} == result.at(1));
			CHECK(ecs::entity_range{14, 17} == result.at(2));
		}

		SECTION("Ranges in B overlap ranges in A") {
			/// a:     ---    ---    ---
			/// b: *****  *****  *****
			std::vector<ecs::entity_range> const vec_a{{4, 6}, {11, 13}, {18, 20}};
			std::vector<ecs::entity_range> const vec_b{{0, 4}, {7, 11}, {14, 18}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{3} == result.size());
			CHECK(ecs::entity_range{5, 6} == result.at(0));
			CHECK(ecs::entity_range{12, 13} == result.at(1));
			CHECK(ecs::entity_range{19, 20} == result.at(2));
		}

		SECTION("Ranges in A overlap multiple ranges in B") {
			/// a: ********* *********
			/// b:  --- ---   --- ---
			std::vector<ecs::entity_range> const vec_a{{0, 8}, {9, 17}};
			std::vector<ecs::entity_range> const vec_b{{1, 3}, {5, 7}, {10, 12}, {14, 16}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{6} == result.size());
			CHECK(ecs::entity_range{0, 0} == result.at(0));
			CHECK(ecs::entity_range{4, 4} == result.at(1));
			CHECK(ecs::entity_range{8, 8} == result.at(2));
			CHECK(ecs::entity_range{9, 9} == result.at(3));
			CHECK(ecs::entity_range{13, 13} == result.at(4));
			CHECK(ecs::entity_range{17, 17} == result.at(5));
		}

		SECTION("Ranges in B overlap multiple ranges in A") {
			/// a:  --- ---   --- ---
			/// b: ********* *********
			std::vector<ecs::entity_range> const vec_a{{1, 3}, {5, 7}, {10, 12}, {14, 16}};
			std::vector<ecs::entity_range> const vec_b{{0, 8}, {9, 17}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{0} == result.size());
		}

		SECTION("One range in B overlaps two ranges in A") {
			/// a: *** ***
			/// b:  -----
			std::vector<ecs::entity_range> const vec_a{{1, 3}, {5, 7}};
			std::vector<ecs::entity_range> const vec_b{{2, 6}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{2} == result.size());
			CHECK(ecs::entity_range{1, 1} == result.at(0));
			CHECK(ecs::entity_range{7, 7} == result.at(1));
		}

		SECTION("One range in A overlaps two ranges in B") {
			/// a:  -----
			/// b: *** ***
			std::vector<ecs::entity_range> const vec_a{{2, 6}};
			std::vector<ecs::entity_range> const vec_b{{1, 3}, {5, 7}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{1} == result.size());
			CHECK(ecs::entity_range{4, 4} == result.at(0));
		}

		SECTION("One range in B removes all but one range in A") {
			/// a: -- -- -- --
			/// b: ********
			std::vector<ecs::entity_range> const vec_a{{0, 1}, {2, 3}, {4, 5}, {6, 7}};
			std::vector<ecs::entity_range> const vec_b{{0, 5}};

			auto const result = ecs::detail::difference_ranges(vec_a, vec_b);

			REQUIRE(size_t{1} == result.size());
			CHECK(ecs::entity_range{6, 7} == result.at(0));
		}
	}

	SECTION("merging ranges") {
		auto constexpr tester = [](std::vector<ecs::entity_range> input, std::vector<ecs::entity_range> const expected) {
			auto constexpr merger = [](ecs::entity_range& a, ecs::entity_range const& b) {
				if (a.adjacent(b)) {
					a = ecs::entity_range::merge(a, b);
					return true;
				} else {
					return false;
				}
			};
			ecs::detail::combine_erase(input, merger);
			CHECK(input == expected);
		};

		// should combine to two entries {0, 3} {5, 8}
		tester({{0, 1}, {2, 3}, {5, 6}, {7, 8}}, {{0, 3}, {5, 8}});

		// reversed. should still combine to two entries {0, 3} {5, 8}
		tester({{7, 8}, {5, 6}, {2, 3}, {0, 1}}, {{5, 8}, {0, 3}});

		// should combine to single entry {0, 8}
		tester({{0, 1}, {2, 3}, {4, 6}, {7, 8}}, {{0, 8}});

		// should not combine
		tester({{0, 1}, {3, 4}, {6, 7}, {9, 10}}, {{0, 1}, {3, 4}, {6, 7}, {9, 10}});
	}
}

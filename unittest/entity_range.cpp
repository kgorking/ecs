#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>

constexpr bool intersect_overflow_test() noexcept {
	constexpr auto max = std::numeric_limits<ecs::detail::entity_type>::max();
	ecs::entity_range r{max - 1, max};

	int64_t counter = 0;
	for (auto const ent : r) {

		// end iterator becomes max+1, which is the same as std::numeric_limits<ecs::entity_type>::min()
		(void)ent;
		counter++;
	}

	return (counter == 2);
}

constexpr bool intersect_empty_with_ranges() noexcept {
	/// a:
	/// b:      ---     ---     ---
	std::vector<ecs::entity_range> const vec_a{};
	std::vector<ecs::entity_range> const vec_b{{5, 7}, {13, 15}, {21, 23}};

	auto const result = ecs::detail::intersect_ranges(vec_a, vec_b);

	return result.empty();
}

constexpr bool intersect_ranges_with_empty() noexcept {
	/// a: *****   *****   *****
	/// b:
	std::vector<ecs::entity_range> const vec_a{{0, 4}, {8, 12}, {16, 20}};
	std::vector<ecs::entity_range> const vec_b{};

	auto const result = ecs::detail::intersect_ranges(vec_a, vec_b);

	return result.empty();
}

constexpr bool intersect_ranges_with_no_overlap() noexcept {
	/// a: *****   *****   *****
	/// b:      ---     ---     ---
	std::vector<ecs::entity_range> const vec_a{{0, 4}, {8, 12}, {16, 20}};
	std::vector<ecs::entity_range> const vec_b{{5, 7}, {13, 15}, {21, 23}};

	auto const result = ecs::detail::intersect_ranges(vec_a, vec_b);

	return result.empty();
}

constexpr bool intersect_ranges_B_contains_ranges_A() noexcept {
	/// a: ***** ***** *****
	/// b:  ---   ---   ---
	std::vector<ecs::entity_range> const vec_a{{0, 4}, {5, 9}, {10, 14}};
	std::vector<ecs::entity_range> const vec_b{{1, 3}, {6, 8}, {11, 13}};

	auto const result = ecs::detail::intersect_ranges(vec_a, vec_b);

	return (size_t{3} == result.size()) && (ecs::entity_range{1, 3}.equals(result.at(0))) &&
		   (ecs::entity_range{6, 8}.equals(result.at(1))) && (ecs::entity_range{11, 13}.equals(result.at(2)));
}

constexpr bool intersect_ranges_A_contains_ranges_B() noexcept {
	/// a:  ---   ---   ---
	/// b: ***** ***** *****
	std::vector<ecs::entity_range> const vec_a{{1, 3}, {6, 8}, {11, 13}};
	std::vector<ecs::entity_range> const vec_b{{0, 4}, {5, 9}, {10, 14}};

	auto const result = ecs::detail::intersect_ranges(vec_a, vec_b);

	return (size_t{3} == result.size()) && (ecs::entity_range{1, 3}.equals(result.at(0))) &&
		   (ecs::entity_range{6, 8}.equals(result.at(1))) && (ecs::entity_range{11, 13}.equals(result.at(2)));
}

constexpr bool intersect_ranges_A_overlaps_ranges_B() noexcept {
	/// a: *****  *****  *****
	/// b:     ---    ---    ---
	std::vector<ecs::entity_range> const vec_a{{0, 4}, {7, 11}, {14, 18}};
	std::vector<ecs::entity_range> const vec_b{{4, 6}, {11, 13}, {18, 20}};

	auto const result = ecs::detail::intersect_ranges(vec_a, vec_b);

	return (size_t{3} == result.size()) && (ecs::entity_range{4, 4}.equals(result.at(0))) &&
		   (ecs::entity_range{11, 11}.equals(result.at(1))) && (ecs::entity_range{18, 18}.equals(result.at(2)));
}

constexpr bool intersect_ranges_B_overlaps_ranges_A() noexcept {
	/// a: *****  *****  *****
	/// b:     ---    ---    ---
	/// a:     ---    ---    ---
	/// b: *****  *****  *****
	std::vector<ecs::entity_range> const vec_a{{4, 6}, {11, 13}, {18, 20}};
	std::vector<ecs::entity_range> const vec_b{{0, 4}, {7, 11}, {14, 18}};

	auto const result = ecs::detail::intersect_ranges(vec_a, vec_b);

	return (size_t{3} == result.size()) && (ecs::entity_range{4, 4}.equals(result.at(0))) &&
		   (ecs::entity_range{11, 11}.equals(result.at(1))) && (ecs::entity_range{18, 18}.equals(result.at(2)));
}

constexpr bool intersect_ranges_A_overlap_multiple_in_B() noexcept {
	/// a: ********* *********
	/// b:  --- ---   --- ---
	std::vector<ecs::entity_range> const vec_a{{0, 8}, {9, 17}};
	std::vector<ecs::entity_range> const vec_b{{1, 3}, {5, 7}, {10, 12}, {14, 16}};

	auto const result = ecs::detail::intersect_ranges(vec_a, vec_b);

	return (size_t{4} == result.size()) && (ecs::entity_range{1, 3} == result.at(0)) && (ecs::entity_range{5, 7} == result.at(1)) &&
		   (ecs::entity_range{10, 12} == result.at(2)) && (ecs::entity_range{14, 16} == result.at(3));
}

constexpr bool intersect_ranges_B_overlap_multiple_in_A() noexcept {
	/// a:  --- ---   --- ---
	/// b: ********* *********
	std::vector<ecs::entity_range> const vec_a{{1, 3}, {5, 7}, {10, 12}, {14, 16}};
	std::vector<ecs::entity_range> const vec_b{{0, 8}, {9, 17}};

	auto const result = ecs::detail::intersect_ranges(vec_a, vec_b);

	return (size_t{4} == result.size()) && (ecs::entity_range{1, 3} == result.at(0)) && (ecs::entity_range{5, 7} == result.at(1)) &&
		   (ecs::entity_range{10, 12} == result.at(2)) && (ecs::entity_range{14, 16} == result.at(3));
}

constexpr bool intersect_one_range_B_overlap_multiple_in_A() noexcept {
	/// a: *** ***
	/// b:  -----
	std::vector<ecs::entity_range> const vec_a{{1, 3}, {5, 7}};
	std::vector<ecs::entity_range> const vec_b{{2, 6}};

	auto const result = ecs::detail::intersect_ranges(vec_a, vec_b);

	return (size_t{2} == result.size()) && (ecs::entity_range{2, 3} == result.at(0)) && (ecs::entity_range{5, 6} == result.at(1));
}

constexpr bool intersect_one_range_A_overlap_multiple_in_B() noexcept {
	/// a:  -----
	/// b: *** ***
	std::vector<ecs::entity_range> const vec_a{{2, 6}};
	std::vector<ecs::entity_range> const vec_b{{1, 3}, {5, 7}};

	auto const result = ecs::detail::intersect_ranges(vec_a, vec_b);

	return (size_t{2} == result.size()) && (ecs::entity_range{2, 3} == result.at(0)) && (ecs::entity_range{5, 6} == result.at(1));
}

constexpr bool diff_empty_range_A() noexcept {
	/// a:
	/// b:      ---     ---     ---
	std::vector<ecs::entity_range> const vec_a{};
	std::vector<ecs::entity_range> const vec_b{{5, 7}, {13, 15}, {21, 23}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return result.empty();
}

constexpr bool diff_empty_range_B() noexcept {
	/// a: *****   *****   *****
	/// b:
	std::vector<ecs::entity_range> const vec_a{{0, 4}, {8, 12}, {16, 20}};
	std::vector<ecs::entity_range> const vec_b{};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return result == vec_a;
}

constexpr bool diff_no_overlaps() noexcept {
	/// a: *****   *****   *****
	/// b:      ---     ---     ---
	std::vector<ecs::entity_range> const vec_a{{0, 4}, {8, 12}, {16, 20}};
	std::vector<ecs::entity_range> const vec_b{{5, 7}, {13, 15}, {21, 23}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return result == vec_a;
}

constexpr bool diff_1_overlap() noexcept {
	/// a: ----
	/// b: *
	std::vector<ecs::entity_range> const vec_a{{0, 3}};
	std::vector<ecs::entity_range> const vec_b{{0, 0}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 1 == result.size() && ecs::entity_range{1, 3}.equals(result.at(0));
}

constexpr bool diff_1_overlap_back() noexcept {
	/// a: ----
	/// b:    *
	std::vector<ecs::entity_range> const vec_a{{0, 3}};
	std::vector<ecs::entity_range> const vec_b{{3, 3}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 1 == result.size() && ecs::entity_range{0, 2}.equals(result.at(0));
}

constexpr bool diff_3_overlap() noexcept {
	/// a: ----
	/// b: ***
	std::vector<ecs::entity_range> const vec_a{{0, 3}};
	std::vector<ecs::entity_range> const vec_b{{0, 0}, {1, 1}, {2, 2}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 1 == result.size() && ecs::entity_range{3, 3}.equals(result.at(0));
}

constexpr bool diff_3_overlap_back() noexcept {
	/// a: ----
	/// b:  ***
	std::vector<ecs::entity_range> const vec_a{{0, 3}};
	std::vector<ecs::entity_range> const vec_b{{1, 1}, {2, 2}, {3, 3}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 1 == result.size() && ecs::entity_range{0, 0}.equals(result.at(0));
}

constexpr bool diff_range_in_B_are_contained_in_ranges_in_A() {
	/// a: ***** ***** *****
	/// b:  ---   ---   ---
	/// r: x   x x   x x   x
	std::vector<ecs::entity_range> const vec_a{{0, 4}, {5, 9}, {10, 14}};
	std::vector<ecs::entity_range> const vec_b{{1, 3}, {6, 8}, {11, 13}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 4 == result.size() && ecs::entity_range{0, 0}.equals(result.at(0)) && ecs::entity_range{4, 5}.equals(result.at(1)) &&
		   ecs::entity_range{9, 10}.equals(result.at(2)) && ecs::entity_range{14, 14}.equals(result.at(3));
}

constexpr bool diff_range_in_A_are_contained_in_ranges_in_B() {
	/// a:  ---   ---   ---
	/// b: ***** ***** *****
	std::vector<ecs::entity_range> const vec_a{{1, 3}, {6, 8}, {11, 13}};
	std::vector<ecs::entity_range> const vec_b{{0, 4}, {5, 9}, {10, 14}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 0 == result.size();
}

constexpr bool diff_range_in_A_overlap_ranges_in_B() {
	/// a: *****  *****  *****
	/// b:     ---    ---    ---
	/// r: xxxx   xxxx   xxxx
	std::vector<ecs::entity_range> const vec_a{{0, 4}, {7, 11}, {14, 18}};
	std::vector<ecs::entity_range> const vec_b{{4, 6}, {11, 13}, {18, 20}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 3 == result.size() && ecs::entity_range{0, 3} == result.at(0) && ecs::entity_range{7, 10} == result.at(1) &&
		   ecs::entity_range{14, 17} == result.at(2);
}

constexpr bool diff_range_in_B_overlap_ranges_in_A() {
	/// a:     ---    ---    ---
	/// b: *****  *****  *****
	/// r:     x      x      x
	std::vector<ecs::entity_range> const vec_a{{4, 6}, {11, 13}, {18, 20}};
	std::vector<ecs::entity_range> const vec_b{{0, 4}, {7, 11}, {14, 18}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 3 == result.size() && ecs::entity_range{5, 6} == result.at(0) && ecs::entity_range{12, 13} == result.at(1) &&
		   ecs::entity_range{19, 20} == result.at(2);
}

constexpr bool diff_ranges_in_A_overlap_multiple_ranges_in_B() {
	/// a: ******************
	/// b:  --- ---  --- ---
	/// r: x   x   xx   x   x
	std::vector<ecs::entity_range> const vec_a{{0, 8}, {9, 17}};
	std::vector<ecs::entity_range> const vec_b{{1, 3}, {5, 7}, {10, 12}, {14, 16}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 5 == result.size() && ecs::entity_range{0, 0} == result.at(0) && ecs::entity_range{4, 4} == result.at(1) &&
		   ecs::entity_range{8, 9} == result.at(2) && ecs::entity_range{13, 13} == result.at(3) &&
		   ecs::entity_range{17, 17} == result.at(4);
}

constexpr bool diff_ranges_in_B_overlap_multiple_ranges_in_A() {
	/// a:  --- ---   --- ---
	/// b: ********* *********
	/// r:
	std::vector<ecs::entity_range> const vec_a{{1, 3}, {5, 7}, {10, 12}, {14, 16}};
	std::vector<ecs::entity_range> const vec_b{{0, 8}, {9, 17}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 0 == result.size();
}

constexpr bool diff_one_range_in_B_overlaps_two_ranges_in_A() {
	/// a: *** ***
	/// b:  -----
	/// r: x     x
	std::vector<ecs::entity_range> const vec_a{{1, 3}, {5, 7}};
	std::vector<ecs::entity_range> const vec_b{{2, 6}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 2 == result.size() && ecs::entity_range{1, 1} == result.at(0) && ecs::entity_range{7, 7} == result.at(1);
}

constexpr bool diff_one_range_in_A_overlaps_two_ranges_in_B() {
	/// a:  -----
	/// b: *** ***
	/// r:    x
	std::vector<ecs::entity_range> const vec_a{{2, 6}};
	std::vector<ecs::entity_range> const vec_b{{1, 3}, {5, 7}};

	auto const result = ecs::detail::difference_ranges(vec_a, vec_b);
	return 1 == result.size() && ecs::entity_range{4, 4} == result.at(0);
}

constexpr bool test_merging_adjacent_ranges() {
	auto constexpr tester = [](std::vector<ecs::entity_range> input, std::vector<ecs::entity_range> const& expected) {
		auto constexpr merger = [](ecs::entity_range& a, ecs::entity_range const b) {
			if (a.adjacent(b)) {
				a = ecs::entity_range::merge(a, b);
				return true;
			} else {
				return false;
			}
		};
		ecs::detail::combine_erase(input, merger);
		return (input == expected);
	};

	// should combine to two entries {0, 3} {5, 8}
	if (!tester({{0, 1}, {2, 3}, {5, 6}, {7, 8}}, {{0, 3}, {5, 8}}))
		return false;

	// reversed. should still combine to two entries {0, 3} {5, 8}
	if (!tester({{7, 8}, {5, 6}, {2, 3}, {0, 1}}, {{5, 8}, {0, 3}}))
		return false;

	// should combine to single entry {0, 8}
	if (!tester({{0, 1}, {2, 3}, {4, 6}, {7, 8}}, {{0, 8}}))
		return false;

	// should not combine
	return tester({{0, 1}, {3, 4}, {6, 7}, {9, 10}}, {{0, 1}, {3, 4}, {6, 7}, {9, 10}});
}

TEST_CASE("entity_range ", "[entity]") {
	SECTION("iterator overflow tests") {
		static_assert(intersect_overflow_test());
		REQUIRE(intersect_overflow_test());
	}

	SECTION("intersection tests") {
		SECTION("Empty range A") {
			static_assert(intersect_empty_with_ranges());
			REQUIRE(intersect_empty_with_ranges());
		}

		SECTION("Empty range B") {
			static_assert(intersect_ranges_with_empty());
			REQUIRE(intersect_ranges_with_empty());
		}

		SECTION("No overlaps between ranges") {
			static_assert(intersect_ranges_with_no_overlap());
			REQUIRE(intersect_ranges_with_no_overlap());
		}

		SECTION("Ranges in A are contained in ranges in B") {
			static_assert(intersect_ranges_B_contains_ranges_A());
			REQUIRE(intersect_ranges_B_contains_ranges_A());
		}

		SECTION("Ranges in B are contained in ranges in A") {
			static_assert(intersect_ranges_A_contains_ranges_B());
			REQUIRE(intersect_ranges_A_contains_ranges_B());
		}

		SECTION("Ranges in A overlap ranges in B") {
			static_assert(intersect_ranges_A_overlaps_ranges_B());
			REQUIRE(intersect_ranges_A_overlaps_ranges_B());
		}

		SECTION("Ranges in B overlap ranges in A") {
			static_assert(intersect_ranges_B_overlaps_ranges_A());
			REQUIRE(intersect_ranges_B_overlaps_ranges_A());
		}

		SECTION("Ranges in A overlap multiple ranges in B") {
			static_assert(intersect_ranges_A_overlap_multiple_in_B());
			REQUIRE(intersect_ranges_A_overlap_multiple_in_B());
		}

		SECTION("Ranges in B overlap multiple ranges in A") {
			static_assert(intersect_ranges_B_overlap_multiple_in_A());
			REQUIRE(intersect_ranges_B_overlap_multiple_in_A());
		}

		SECTION("One range in B overlaps two ranges in A") {
			static_assert(intersect_one_range_B_overlap_multiple_in_A());
			REQUIRE(intersect_one_range_B_overlap_multiple_in_A());
		}

		SECTION("One range in A overlaps two ranges in B") {
			static_assert(intersect_one_range_A_overlap_multiple_in_B());
			REQUIRE(intersect_one_range_A_overlap_multiple_in_B());
		}
	}

	SECTION("difference between ranges") {
		SECTION("Empty range A") {
			static_assert(diff_empty_range_A());
			REQUIRE(diff_empty_range_A());
		}

		SECTION("Empty range B") {
			static_assert(diff_empty_range_B());
			REQUIRE(diff_empty_range_B());
		}

		SECTION("No overlaps between ranges") {
			static_assert(diff_no_overlaps());
			REQUIRE(diff_no_overlaps());
		}

		SECTION("Remove 1 entity from front of A") {
			static_assert(diff_1_overlap());
			REQUIRE(diff_1_overlap());
		}

		SECTION("Remove 1 entity from back of A") {
			static_assert(diff_1_overlap_back());
			REQUIRE(diff_1_overlap_back());
		}

		SECTION("Remove 3 entities from front of A") {
			static_assert(diff_3_overlap());
			REQUIRE(diff_3_overlap());
		}

		SECTION("Remove 3 entities from back of A") {
			static_assert(diff_3_overlap_back());
			REQUIRE(diff_3_overlap_back());
		}

		SECTION("Ranges in B are contained in ranges in A") {
			static_assert(diff_range_in_B_are_contained_in_ranges_in_A());
			REQUIRE(diff_range_in_B_are_contained_in_ranges_in_A());
		}

		SECTION("Ranges in A are contained in ranges in B") {
			static_assert(diff_range_in_A_are_contained_in_ranges_in_B());
			REQUIRE(diff_range_in_A_are_contained_in_ranges_in_B());
		}

		SECTION("Ranges in A overlap ranges in B") {
			static_assert(diff_range_in_A_overlap_ranges_in_B());
			REQUIRE(diff_range_in_A_overlap_ranges_in_B());
		}

		SECTION("Ranges in B overlap ranges in A") {
			static_assert(diff_range_in_B_overlap_ranges_in_A());
			REQUIRE(diff_range_in_B_overlap_ranges_in_A());
		}

		SECTION("Ranges in A overlap multiple ranges in B") {
			static_assert(diff_ranges_in_A_overlap_multiple_ranges_in_B());
			REQUIRE(diff_ranges_in_A_overlap_multiple_ranges_in_B());
		}

		SECTION("Ranges in B overlap multiple ranges in A") {
			static_assert(diff_ranges_in_B_overlap_multiple_ranges_in_A());
			REQUIRE(diff_ranges_in_B_overlap_multiple_ranges_in_A());
		}

		SECTION("One range in B overlaps two ranges in A") {
			static_assert(diff_one_range_in_B_overlaps_two_ranges_in_A());
			REQUIRE(diff_one_range_in_B_overlaps_two_ranges_in_A());
		}

		SECTION("One range in A overlaps two ranges in B") {
			static_assert(diff_one_range_in_A_overlaps_two_ranges_in_B());
			REQUIRE(diff_one_range_in_A_overlaps_two_ranges_in_B());
		}
	}

	SECTION("merging ranges") {
		static_assert(test_merging_adjacent_ranges());
		REQUIRE(test_merging_adjacent_ranges());
	}
}

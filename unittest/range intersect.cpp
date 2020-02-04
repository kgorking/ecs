#include <ecs/ecs.h>
#include "catch.hpp"

TEST_CASE("entity_range intersection tests", "[entity]")
{
	// Intersects two ranges of entities
	// This lambda is used in system_impl
	auto const intersector = [](std::vector<ecs::entity_range> const& vec_a, std::vector<ecs::entity_range> const& vec_b) -> std::vector<ecs::entity_range> {
		if (vec_a.empty() || vec_b.empty()) {
			return {};
		}

		std::vector<ecs::entity_range> result;

		// Iterate over the current intersection result set and match it against the incoming set
		auto it_a = vec_a.begin();
		auto it_b = vec_b.begin();

		while (it_a != vec_a.end() && it_b != vec_b.end()) {
			if (it_a->overlaps(*it_b)) {
				result.push_back(ecs::entity_range::intersect(*it_a, *it_b));
			}

			if (it_a->last() < it_b->last()) {
				++it_a;
			}
			else if (it_b->last() < it_a->last()) {
				++it_b;
			}
			else {
				++it_a;
				++it_b;
			}
		}

		return result;
	};

	SECTION("No overlaps between ranges")
	{
		/// a: *****   *****   *****
		/// b:      ---     ---     ---
		std::vector<ecs::entity_range> vec_a{ {0,4},  {8,12},    {16,20} };
		std::vector<ecs::entity_range> vec_b{     {5,7},   {13,15},    {21,23} };

		auto result = intersector(vec_a, vec_b);

		REQUIRE(result.empty());
	}

	SECTION("Ranges in B are contained in ranges in A")
	{
		/// a: ***** ***** *****
		/// b:  ---   ---   ---
		std::vector<ecs::entity_range> vec_a{ {0,4}, {5,9}, {10,14} };
		std::vector<ecs::entity_range> vec_b{ {1,3}, {6,8}, {11,13} };

		auto result = intersector(vec_a, vec_b);

		REQUIRE(3 == result.size());
		CHECK(ecs::entity_range{ 1,3 }.equals(result.at(0)));
		CHECK(ecs::entity_range{ 6,8 }.equals(result.at(1)));
		CHECK(ecs::entity_range{ 11,13 }.equals(result.at(2)));
	}

	SECTION("Ranges in A are contained in ranges in B")
	{
		/// a:  ---   ---   ---
		/// b: ***** ***** *****
		std::vector<ecs::entity_range> vec_a{ {1,3}, {6,8}, {11,13} };
		std::vector<ecs::entity_range> vec_b{ {0,4}, {5,9}, {10,14} };

		auto result = intersector(vec_a, vec_b);

		REQUIRE(3 == result.size());
		CHECK(ecs::entity_range{ 1,3 }.equals(result.at(0)));
		CHECK(ecs::entity_range{ 6,8 }.equals(result.at(1)));
		CHECK(ecs::entity_range{ 11,13 }.equals(result.at(2)));
	}

	SECTION("Ranges in A overlap ranges in B")
	{
		/// a: *****  *****  *****
		/// b:     ---    ---    ---
		std::vector<ecs::entity_range> vec_a{ {0,4}, {7,11},  {14,18} };
		std::vector<ecs::entity_range> vec_b{ {4,6}, {11,13}, {18,20} };

		auto result = intersector(vec_a, vec_b);

		REQUIRE(3 == result.size());
		CHECK(ecs::entity_range{ 4,4 } == result.at(0));
		CHECK(ecs::entity_range{ 11,11 } == result.at(1));
		CHECK(ecs::entity_range{ 18,18 } == result.at(2));
	}

	SECTION("Ranges in B overlap ranges in A")
	{
		/// a:     ---    ---    ---
		/// b: *****  *****  *****
		std::vector<ecs::entity_range> vec_a{ {4,6}, {11,13}, {18,20} };
		std::vector<ecs::entity_range> vec_b{ {0,4}, {7,11},  {14,18} };

		auto result = intersector(vec_a, vec_b);

		REQUIRE(3 == result.size());
		CHECK(ecs::entity_range{ 4,4 } == result.at(0));
		CHECK(ecs::entity_range{ 11,11 } == result.at(1));
		CHECK(ecs::entity_range{ 18,18 } == result.at(2));
	}

	SECTION("Ranges in A overlap multiple ranges in B")
	{
		/// a: ********* *********
		/// b:  --- ---   --- ---
		std::vector<ecs::entity_range> vec_a{ {0,8}, {9,17} };
		std::vector<ecs::entity_range> vec_b{ {1,3}, {5,7}, {10,12}, {14,16} };

		auto result = intersector(vec_a, vec_b);

		REQUIRE(4 == result.size());
		CHECK(ecs::entity_range{ 1,3 } == result.at(0));
		CHECK(ecs::entity_range{ 5,7 } == result.at(1));
		CHECK(ecs::entity_range{ 10,12 } == result.at(2));
		CHECK(ecs::entity_range{ 14,16 } == result.at(3));
	}

	SECTION("Ranges in B overlap multiple ranges in A")
	{
		/// a:  --- ---   --- ---
		/// b: ********* *********
		std::vector<ecs::entity_range> vec_a{ {1,3}, {5,7}, {10,12}, {14,16} };
		std::vector<ecs::entity_range> vec_b{ {0,8},       {9,17} };

		auto result = intersector(vec_a, vec_b);

		REQUIRE(4 == result.size());
		CHECK(ecs::entity_range{ 1,3 } == result.at(0));
		CHECK(ecs::entity_range{ 5,7 } == result.at(1));
		CHECK(ecs::entity_range{ 10,12 } == result.at(2));
		CHECK(ecs::entity_range{ 14,16 } == result.at(3));
	}

	SECTION("One range in B overlaps two ranges in A")
	{
		/// a: *** ***
		/// b:  -----
		std::vector<ecs::entity_range> vec_a{ {1,3},{5,7} };
		std::vector<ecs::entity_range> vec_b{ {2,    6} };

		auto result = intersector(vec_a, vec_b);

		REQUIRE(2 == result.size());
		CHECK(ecs::entity_range{ 2,3 } == result.at(0));
		CHECK(ecs::entity_range{ 5,6 } == result.at(1));
	}

	SECTION("One range in A overlaps two ranges in B")
	{
		/// a:  -----
		/// b: *** ***
		std::vector<ecs::entity_range> vec_a{ {2,    6} };
		std::vector<ecs::entity_range> vec_b{ {1,3},{5,7} };

		auto result = intersector(vec_a, vec_b);

		REQUIRE(2 == result.size());
		CHECK(ecs::entity_range{ 2,3 } == result.at(0));
		CHECK(ecs::entity_range{ 5,6 } == result.at(1));
	}
}

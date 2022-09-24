#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>
#include <ecs/parent.h>
#include <unordered_set>
#include <vector>

TEST_CASE("Hierarchies") {
	SECTION("can extract parent info") {
		ecs::runtime ecs;

		// The root
		ecs.add_component({1}, int{});

		// The children
		ecs.add_component(2, ecs::parent{1}, short{10});
		ecs.add_component(3, ecs::parent{1}, long{20});
		ecs.add_component(4, ecs::parent{1}, float{30});

		// The grandchildren
		ecs.add_component({5, 7}, ecs::parent{2});	 // short children, parent 2 has a short
		ecs.add_component({8, 10}, ecs::parent{3});	 // long children, parent 3 has a long
		ecs.add_component({11, 13}, ecs::parent{4}); // float children, parent 4 has a float

		ecs.commit_changes();

		// verify parent types
		std::atomic_int count_short = 0, count_long = 0, count_float = 0;
		ecs.make_system([&count_short](ecs::entity_id id, ecs::parent<short> const& p) {
			CHECK((id >= 5 && id <= 7)); // check id value
			CHECK(p.get<short>() == 10); // check parent value
			count_short++;
		});
		ecs.make_system([&count_long](ecs::entity_id id, ecs::parent<long> const& p) {
			CHECK((id >= 8 && id <= 10));
			CHECK(p.get<long>() == 20);
			count_long++;
		});
		ecs.make_system([&count_float](ecs::entity_id id, ecs::parent<float> const& p) {
			CHECK((id >= 11 && id <= 13));
			CHECK(p.get<float>() == 30.f);
			count_float++;
		});

		ecs.update();

		CHECK(count_short == 3);
		CHECK(count_long == 3);
		CHECK(count_float == 3);
	}

	SECTION("are traversed correctly") {
		ecs::runtime ecs;

		/*     ______1_________              */
		/*    /      |         \             */
		/*   4       3          2            */
		/*  /|\     /|\       / | \          */
		/* 5 6 7   8 9 10   11  12 13        */
		/* |         |             |         */
		/* 14        15            16        */

		// The root
		ecs.add_component({1}, int{});

		// The children
		ecs.add_component(4, ecs::parent{1});
		ecs.add_component(3, ecs::parent{1});
		ecs.add_component(2, ecs::parent{1});

		// The grandchildren
		ecs.add_component({5, 7}, ecs::parent{4});
		ecs.add_component({8, 10}, ecs::parent{3});
		ecs.add_component({11, 13}, ecs::parent{2});

		// The great-grandchildren
		ecs.add_component(14, ecs::parent{5});
		ecs.add_component(15, ecs::parent{9});
		ecs.add_component(16, ecs::parent{13});

		// The system to verify the traversal order
		std::unordered_set<int> traversal_order;
		traversal_order.insert(1);
		ecs.make_system<ecs::opts::not_parallel>([&traversal_order](ecs::entity_id id, ecs::parent<> p) {
			// Make sure parents are processed before the children
			CHECK(false == traversal_order.contains(id));
			CHECK(true == traversal_order.contains(p));
			traversal_order.insert(id);
		});

		ecs.update();

		// Make sure all children where visited
		CHECK(std::ssize(traversal_order) == (1 + ecs.get_component_count<ecs::detail::parent_id>()));
	}

	SECTION("works on multiple trees") {
		ecs::runtime ecs;

		//
		//
		//   4       3          2
		//  /|\     /|\       / | \
        // 5 6 7   8 9 10   11  12 13
		// |         |             |
		// 14        15            16

		// The roots
		ecs.add_component(4, int{1});
		ecs.add_component(3, int{1});
		ecs.add_component(2, int{1});

		// The children
		ecs.add_component({5, 7}, ecs::parent{4});
		ecs.add_component({8, 10}, ecs::parent{3});
		ecs.add_component({11, 13}, ecs::parent{2});

		// The grandchildren
		ecs.add_component(14, ecs::parent{5});
		ecs.add_component(15, ecs::parent{9});
		ecs.add_component(16, ecs::parent{13});

		// The system to verify the traversal order
		std::unordered_set<int> traversal_order;
		traversal_order.insert(2);
		traversal_order.insert(3);
		traversal_order.insert(4);
		ecs.make_system<ecs::opts::not_parallel>([&](ecs::entity_id id, ecs::parent<> p) {
			// Make sure parents are processed before the children
			CHECK(false == traversal_order.contains(id));
			CHECK(true == traversal_order.contains(p));
			traversal_order.insert(id);
		});

		ecs.update();

		// Make sure all children where visited
		CHECK(std::ssize(traversal_order) == (3 + ecs.get_component_count<ecs::detail::parent_id>()));
	}

	SECTION("works on lots of trees") {
		ecs::runtime ecs;

		auto const nentities = 256;

		// The set to verify the traversal order
		std::unordered_set<int> traversal_order;

		ecs::detail::entity_type i = 0;
		while (i < nentities) {
			traversal_order.insert(i + 0);
			ecs.add_component({i + 0}, int{});
			ecs.add_component({i + 1, i + 7}, int{}, ecs::parent{i + 0});

			i += 8;
		}

		ecs.commit_changes();

		ecs.make_system<ecs::opts::not_parallel>([&](ecs::entity_id id, ecs::parent<> p) {
			CHECK(id >= 0);
			CHECK(id <= nentities);

			// Make sure parents are processed before the children
			CHECK(false == traversal_order.contains(id));
			CHECK(true == traversal_order.contains(p));

			traversal_order.insert(id);
		});

		ecs.update();

		// Make sure all children where visited
		CHECK(traversal_order.size() == size_t{nentities});
	}

	SECTION("works on multiple trees in parallel") {
		ecs::runtime ecs;

		//
		//
		//   4       3          2
		//  /|\     /|\       / | \
        // 5 6 7   8 9 10   11  12 13
		// |         |             |
		// 14        15            16

		// The roots
		ecs.add_component(4, int{1});
		ecs.add_component(3, int{1});
		ecs.add_component(2, int{1});

		// The children
		ecs.add_component({5, 7}, ecs::parent{4});
		ecs.add_component({8, 10}, ecs::parent{3});
		ecs.add_component({11, 13}, ecs::parent{2});

		// The grandchildren
		ecs.add_component(14, ecs::parent{5});
		ecs.add_component(15, ecs::parent{9});
		ecs.add_component(16, ecs::parent{13});

		// The system to verify the traversal order
		std::unordered_set<int> traversal_order;
		traversal_order.insert(2);
		traversal_order.insert(3);
		traversal_order.insert(4);
		ecs.make_system<ecs::opts::not_parallel>([&](ecs::entity_id id, ecs::parent<> p) {
			// Make sure parents are processed before the children
			CHECK(false == traversal_order.contains(id));
			CHECK(true == traversal_order.contains(p));
			traversal_order.insert(id);
		});

		ecs.update();

		// Make sure all children where visited
		CHECK(std::ssize(traversal_order) == (3 + ecs.get_component_count<ecs::detail::parent_id>()));
	}

	SECTION("can be built bottoms-up") {
		ecs::runtime ecs;

		// 14        15            16
		// |         |             |
		// 5 6 7   8 9 10   11  12 13
		//  \|/     \|/       \ | /
		//   4       3          2
		//    \______|_________/
		//           1

		// The great-grandchildren
		ecs.add_component(14, ecs::parent{5});
		ecs.add_component(15, ecs::parent{9});
		ecs.add_component(16, ecs::parent{13});

		// The grandchildren
		ecs.add_component({5, 7}, ecs::parent{4});
		ecs.add_component({8, 10}, ecs::parent{3});
		ecs.add_component({11, 13}, ecs::parent{2});

		// The children
		ecs.add_component(4, ecs::parent{1});
		ecs.add_component(3, ecs::parent{1});
		ecs.add_component(2, ecs::parent{1});

		// The root
		ecs.add_component({1}, int{});

		// The system to verify the traversal order
		std::unordered_set<int> traversal_order;
		traversal_order.insert(1);
		ecs.make_system<ecs::opts::not_parallel>([&traversal_order](ecs::entity_id id, ecs::parent<> p) {
			// Make sure parents are processed before the children
			CHECK(false == traversal_order.contains(id));
			CHECK(true == traversal_order.contains(p));
			traversal_order.insert(id);
		});

		ecs.update();

		// Make sure all children where visited
		CHECK(std::ssize(traversal_order) == (1 + ecs.get_component_count<ecs::detail::parent_id>()));
	}

	SECTION("can be built in reverse") {
		ecs::runtime ecs;

		//      ______16________
		//     /      |         \
        //    13      14        15
		//   /| \    /|\        /|\
        // 10 11 12 7 8 9      4 5 6
		//  |         |            |
		//  3         2            1

		// The root
		ecs.add_component({16}, int{});

		// The children
		ecs.add_component(15, ecs::parent{16});
		ecs.add_component(14, ecs::parent{16});
		ecs.add_component(13, ecs::parent{16});

		// The grandchildren
		ecs.add_component({10, 12}, ecs::parent{13});
		ecs.add_component({7, 9}, ecs::parent{14});
		ecs.add_component({4, 6}, ecs::parent{15});

		// The great-grandchildren
		ecs.add_component(3, ecs::parent{10});
		ecs.add_component(2, ecs::parent{8});
		ecs.add_component(1, ecs::parent{6});

		// The system to verify the traversal order
		std::unordered_map<int, bool> traversal_order;
		traversal_order[16] = true;
		ecs.make_system<ecs::opts::not_parallel>([&traversal_order](ecs::entity_id id, ecs::parent<> p) {
			// Make sure parents are processed before the children
			CHECK(false == traversal_order.contains(id));
			CHECK(true == traversal_order.contains(p));
			traversal_order[id] = true;
		});

		ecs.update();

		// Make sure all children where visited
		CHECK(std::ssize(traversal_order) == (1 + ecs.get_component_count<ecs::detail::parent_id>()));
	}

	SECTION("interactions with parents are correct") {
		ecs::runtime ecs;

		auto const nentities = 256;

		ecs::detail::entity_type id = 0;
		while (id < nentities) {
			ecs.add_component({id + 0}, int{});

			auto p = ecs::parent{id + 0};
			ecs.add_component({id + 1, id + 7}, int{}, p);

			id += 8;
		}

		ecs.commit_changes();

		using namespace ecs::opts;
		ecs.make_system<not_parallel>([](ecs::parent<int>& p) { p.get<int>() += 1; });

		int num_correct = 0;
		ecs.make_system<not_parallel>([&num_correct](int i, ecs::parent<>*) {
			if (i == 7)
				num_correct += 1;
		});

		ecs.update();

		CHECK(num_correct == nentities / 8);
	}

	SECTION("can filter on parents") {
		ecs::runtime ecs;

		ecs.add_component({0}, int{});
		ecs.add_component({1}, int{}, ecs::parent{0});

		// This system is not a hierarchy
		bool filter_works = false;
		ecs.make_system<ecs::opts::not_parallel>(
			[&filter_works](ecs::entity_id id, int, ecs::parent<>*) { // run on entities with an int and no parent
				CHECK(id == 0);
				filter_works = true;
			});

		ecs.update();

		CHECK(filter_works);
	}

	SECTION("can filter on parent subtypes") { // parent<int, float*>
		ecs::runtime ecs;

		ecs.add_component({0}, int{11}, float{});
		ecs.add_component({1}, int{22}, ecs::parent{0});
		ecs.add_component({2}, int{33}, float{}, ecs::parent{1});

		// run on entities with an int and a parent with no float
		ecs.make_system<ecs::opts::not_parallel>([](ecs::entity_id id, int i, ecs::parent<float*> p) {
			CHECK(id == 2);
			CHECK(i == 33);
			CHECK(p.id() == 1);
		});

		ecs.update();
	}
}

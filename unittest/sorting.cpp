#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>
#include <functional>

TEST_CASE("Sorting") {
	ecs::runtime ecs;

	std::random_device rd;
	std::mt19937 gen{rd()};

	std::vector<int> ints(10);
	std::iota(ints.begin(), ints.end(), 0);
	std::shuffle(ints.begin(), ints.end(), gen);

	ecs.add_component({0, 9}, ints);
	ecs.commit_changes();

	int test = std::numeric_limits<int>::min();
	auto& asc = ecs.make_system<ecs::opts::not_parallel>(
		[&test](int const& i) {
			CHECK(test <= i);
			test = i;
		},
		std::less<int>());
	asc.run();

	test = std::numeric_limits<int>::max();
	auto& dec = ecs.make_system<ecs::opts::not_parallel>(
		[&test](int const& i) {
			CHECK(test >= i);
			test = i;
		},
		std::greater<int>());
	dec.run();

	// modify the components and re-check
	auto& mod = ecs.make_system([&gen](int& i) {
		i = gen();
	});
	mod.run();

	test = std::numeric_limits<int>::min();
	asc.run();

	test = std::numeric_limits<int>::max();
	dec.run();
}
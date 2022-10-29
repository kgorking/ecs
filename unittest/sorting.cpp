#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/ecs.h>
#include <functional>

TEST_CASE("Sorting") {
	ecs::runtime ecs;

	static constexpr std::size_t num_components = 10000;

	std::vector<unsigned> ints(num_components);
	std::iota(ints.begin(), ints.end(), 0);

	std::random_device rd;
	std::mt19937 gen{rd()};
	std::shuffle(ints.begin(), ints.end(), gen);

	ecs.add_component({1, num_components}, ints);
	ecs.commit_changes();

	unsigned test = std::numeric_limits<unsigned>::min();
	auto& asc = ecs.make_system<ecs::opts::not_parallel, ecs::opts::manual_update>(
		[&test](unsigned const& i) {
			CHECK(test <= i);
			test = i;
		},
		std::less<unsigned>());
	asc.run();

	test = std::numeric_limits<unsigned>::max();
	auto& dec = ecs.make_system<ecs::opts::not_parallel, ecs::opts::manual_update>(
		[&test](unsigned const& i) {
			CHECK(test >= i);
			test = i;
		},
		std::greater<unsigned>());
	dec.run();

	// modify the components and re-check
	auto& mod = ecs.make_system<ecs::opts::not_parallel, ecs::opts::manual_update>([&gen](unsigned& i) {
		i = gen();
	});
	mod.run();

	test = std::numeric_limits<unsigned>::min();
	asc.run();

	test = std::numeric_limits<unsigned>::max();
	dec.run();
}
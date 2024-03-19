#include <catch2/catch_test_macros.hpp>
#include <ecs/detail/array_scatter_allocator.h>

using ecs::detail::array_scatter_allocator;

TEST_CASE("Array-scatter allocator") {
	array_scatter_allocator<int> alloc;

	int total_alloc = 0;
	int const pools_used = alloc.allocate(24, [&](int* , int count) {
		total_alloc += count;
	});

	REQUIRE(total_alloc == 24);
	REQUIRE(pools_used == 2);
}

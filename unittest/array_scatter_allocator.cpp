#include <catch2/catch_test_macros.hpp>
#include <ecs/detail/array_scatter_allocator.h>

using ecs::detail::array_scatter_allocator;

TEST_CASE("Array-scatter allocator") {
	array_scatter_allocator<int> alloc;
	auto vec = alloc.allocate(10);			// +10
	alloc.deallocate(vec[0].subspan(3, 4)); // -4
	vec = alloc.allocate(20);				// +20
}

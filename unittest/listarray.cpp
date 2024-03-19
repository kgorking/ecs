#include <catch2/catch_test_macros.hpp>
#include <ecs/detail/listarray.h>

using ecs::detail::listarray;

TEST_CASE("List array") {
	listarray<int> la;
}

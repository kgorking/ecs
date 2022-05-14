#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/detail/type_list.h>

#include <ecs/detail/options.h>
#include <ecs/parent.h>

using namespace ecs::detail;

using tl = type_list<int, float, char, char[2], double>;
//using tl = type_list<int, char>;
/*
static_assert(std::is_same_v<type_list_at<0, tl>, int>);
static_assert(std::is_same_v<type_list_at<1, tl>, float>);
static_assert(std::is_same_v<type_list_at<2, tl>, char>);
static_assert(std::is_same_v<type_list_at<3, tl>, char[2]>);
static_assert(std::is_same_v<type_list_at<4, tl>, double>);
*/

using parent_test_1 = ecs::parent<int, float>;
using parent_test_2 = ecs::parent<>;
static_assert(ecs::detail::is_parent<parent_test_1>::value);
static_assert(ecs::detail::is_parent<parent_test_2>::value);

static_assert(std::is_same_v<void, test_option_type_or<is_parent, type_list<int, char>, void>>);
static_assert(!std::is_same_v<void, test_option_type_or<is_parent, type_list<int, parent_test_1, char>, void>>);
static_assert(std::is_same_v<void, test_option_type_or<is_parent, type_list<int, parent_test_1*, char>, void>>);
static_assert(!std::is_same_v<void, test_option_type_or<is_parent, type_list<int, parent_test_1&, char>, void>>);

TEST_CASE("type_list") {
    SECTION("run_if") {
        using TL = ecs::detail::type_list<int, ecs::opts::group<1>>;
        using TL2 = ecs::detail::type_list<int, float>;

        int runs = 0;
        int ret_run = ecs::detail::run_if<ecs::detail::is_group, TL>([&]<typename>() {
            runs += 1;
            return runs;
        });
        ecs::detail::run_if<ecs::detail::is_group, TL2>([&]<typename>() {
            runs += 1;
        });

        CHECK(1 == runs);
        CHECK(1 == ret_run);
    }
}

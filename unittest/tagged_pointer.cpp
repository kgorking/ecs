#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/detail/tagged_pointer.h>
#include <bit>

template<typename T>
using tagged_pointer = ecs::detail::tagged_pointer<T>;

// Tests to make sure tagged_pointer behaves as expected.
TEST_CASE("tagged_pointer spec") {
	SECTION("default constructed are not tagged") {
		tagged_pointer<int> tp;
		REQUIRE(!tp.tag());
	}

	SECTION("default constructed can be tagged") {
		tagged_pointer<int> tp;
		tp.set_tag(true);
		REQUIRE(tp.tag());
	}

	SECTION("tags carry over on copy-construction") {
		tagged_pointer<int> tp;
		tp.set_tag(true);
		auto const tp2 = tp;
		REQUIRE(tp2.tag());
	}

	SECTION("tags carry over on move-construction") {
		tagged_pointer<int> tp;
		tp.set_tag(true);
		auto const tp2 = std::move(tp);
		REQUIRE(tp2.tag());
	}

	SECTION("pointers are not changed by tags") {
		int const* i_ptr = nullptr;
		tagged_pointer<int const> tp(i_ptr);
		tp.set_tag(true);
		int const* i2_ptr = tp.pointer();
		REQUIRE(nullptr == i2_ptr);
	}

	SECTION("removing tags does not alter the pointer") {
		int const* i_ptr = nullptr;
		tagged_pointer<int const> tp(i_ptr);
		tp.set_tag(true);
		tp.set_tag(false);
		REQUIRE(!tp.tag());

		int const* i2_ptr = tp.pointer();
		REQUIRE(nullptr == i2_ptr);
	}

	SECTION("dunno") {
		int ip[2]{3, 5};
		tagged_pointer<int> tp[2]{&ip[0], &ip[1]};
		tp[0].set_tag(false);
		tp[1].set_tag(true);

		auto tp1 = *std::bit_cast<tagged_pointer<int>*>(tp+1);
		CHECK(tp1.pointer() == tp[1].pointer());
		CHECK(tp1.tag());
	}
}

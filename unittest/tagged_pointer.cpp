#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <ecs/detail/tagged_pointer.h>
#include <bit>

template<typename T>
using tagged_pointer = ecs::detail::tagged_pointer<T>;

// Tests to make sure tagged_pointer behaves as expected.
TEST_CASE("tagged_pointer spec") {
	SECTION("default constructed are not tagged") {
		tagged_pointer<int64_t> tp;
		REQUIRE(!tp.test_bit1());
	}

	SECTION("default constructed can be tagged") {
		tagged_pointer<int64_t> tp;
		tp.set_tag(5);
		REQUIRE(5 == tp.get_tag());
	}

	SECTION("tags carry over on copy-construction") {
		tagged_pointer<int64_t> tp;
		tp.set_bit1();
		auto const tp2 = tp;
		REQUIRE(tp2.test_bit1());
	}

	SECTION("tags carry over on move-construction") {
		tagged_pointer<int64_t> tp;
		tp.set_bit2();
		auto const tp2 = std::move(tp);
		REQUIRE(tp2.test_bit2());
	}

	SECTION("pointers are not changed by tags") {
		int64_t const* i_ptr = nullptr;
		tagged_pointer<int64_t const> tp(i_ptr);
		tp.set_bit3();
		int64_t const* i2_ptr = tp.pointer();
		REQUIRE(nullptr == i2_ptr);
	}

	SECTION("removing tags does not alter the pointer") {
		int64_t const i = 44;
		int64_t const* i_ptr = &i;
		tagged_pointer tp(i_ptr);

		tp.set_bit3();
		REQUIRE(tp.test_bit3());

		tp.clear_bit3();
		REQUIRE(!tp.test_bit3());

		int64_t const* i2_ptr = tp.pointer();
		REQUIRE(&i == i2_ptr);
		REQUIRE(i == *i2_ptr);
	}
}

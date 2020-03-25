#include <ecs/ecs.h>
#include "catch.hpp"

struct runtime_ctr_counter {
	inline static size_t def_ctr_count = 0;
	inline static size_t ctr_count = 0;
	inline static size_t copy_count = 0;
	inline static size_t move_count = 0;
	inline static size_t dtr_count = 0;

	runtime_ctr_counter() noexcept {
		def_ctr_count++;
		ctr_count++;
	}
	runtime_ctr_counter(runtime_ctr_counter const& /*other*/) {
		copy_count++;
		ctr_count++;
	}
	runtime_ctr_counter(runtime_ctr_counter&& /*other*/) noexcept {
		move_count++;
		ctr_count++;
	}
	~runtime_ctr_counter() {
		dtr_count++;
	}

	runtime_ctr_counter& operator=(runtime_ctr_counter&&) = default;
	runtime_ctr_counter& operator=(runtime_ctr_counter const&) = default;
};

TEST_CASE("The runtime interface") {
	SECTION("Does perfect forwarding correctly") {
		ecs::add_component({ 0,9 }, runtime_ctr_counter{});
		ecs::commit_changes();

		REQUIRE(runtime_ctr_counter::def_ctr_count == 1);
		REQUIRE(runtime_ctr_counter::move_count == 2);
		REQUIRE(runtime_ctr_counter::dtr_count == 1 + 2);
		REQUIRE(runtime_ctr_counter::copy_count == 10);

		ecs::detail::_context.reset();
		REQUIRE(runtime_ctr_counter::dtr_count == 1 + 2 + 10);
	}

	SECTION("allocates storage as needed") {
		ecs::detail::_context.reset();

		// Use a local struct to avoid it possibly
		// already existing from another unittest
		struct S { size_t c; };

		// Add a system-less component to an entity
		ecs::add_component(0, S{ 0 });
		ecs::commit_changes();
		REQUIRE(ecs::get_component_count<S>() == 1);
	}
}

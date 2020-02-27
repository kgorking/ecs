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

TEST_CASE("Test the runtime interface", "[runtime]") {
	SECTION("Perfect forwarding") {

		ecs::add_component({ 0,9 }, runtime_ctr_counter{});
		// 1 4 0 3 3
		// 1 3 0 2 2
		// 1 2 0 1 1

		ecs::commit_changes();
	}
}

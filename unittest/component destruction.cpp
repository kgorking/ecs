#include <ecs/ecs.h>
#include "catch.hpp"

struct C_GlobalCounter {
	inline static size_t ctr_count = 0;
	inline static size_t copy_count = 0;
	inline static size_t move_count = 0;
	inline static size_t dtr_count = 0;

	C_GlobalCounter() {
		ctr_count++;
	}
	C_GlobalCounter(C_GlobalCounter const&) {
		ctr_count++;
		copy_count++;
	}
	C_GlobalCounter(C_GlobalCounter &&) noexcept {
		//ctr_count++;
		move_count++;
	}
	~C_GlobalCounter() {
		dtr_count++;
	}

	C_GlobalCounter& operator=(C_GlobalCounter&&) = default;
	C_GlobalCounter& operator=(C_GlobalCounter const&) = default;
};

TEST_CASE("Component destruction")
{
	// Component pools work with uninitialized memory internally,
	// so make sure objects are cleanup up properly
	SECTION("Test destruction")
	{
		ecs::runtime::reset();

		// Sets up the internal storage for C_GlobalCounter components
		ecs::runtime::init_components<C_GlobalCounter>();

		for (auto i = 0u; i <= 50; i++)
			ecs::add_component(i, C_GlobalCounter{});
		ecs::commit_changes();

		ecs::remove_component_range<C_GlobalCounter>(0, 50);
		ecs::commit_changes();

		REQUIRE(C_GlobalCounter::copy_count == 51);
	}
}

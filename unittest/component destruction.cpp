#include <ecs/ecs.h>
#include "catch.hpp"

struct global_counter {
	inline static size_t def_ctr_count = 0;
	inline static size_t ctr_count = 0;
	inline static size_t copy_count = 0;
	inline static size_t move_count = 0;
	inline static size_t dtr_count = 0;

	global_counter() {
		def_ctr_count++;
		ctr_count++;
	}
	global_counter(global_counter const&) {
		copy_count++;
		ctr_count++;
	}
	global_counter(global_counter &&) noexcept {
		move_count++;
		ctr_count++;
	}
	~global_counter() {
		dtr_count++;
	}

	global_counter& operator=(global_counter&&) = default;
	global_counter& operator=(global_counter const&) = default;
};

TEST_CASE("Test that components are constructed/copied/destroyed properly")
{
	ecs::runtime::reset();

	// Sets up the internal storage for global_counter components
	ecs::runtime::init_components<global_counter>();

	for (auto i = 0u; i <= 50; i++)
		ecs::add_component(i, global_counter{});
	ecs::commit_changes();

	ecs::remove_component_range<global_counter>({ 0, 50 });
	ecs::commit_changes();

	CHECK(global_counter::copy_count == 51);
	CHECK(global_counter::ctr_count == global_counter::dtr_count);
}

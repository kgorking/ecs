#include <ecs/ecs.h>
#include <numeric>
#include <exception>
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

// Override the default handler for contract violations.
struct unittest_handler {
	void assertion_failed(char const* , char const* msg)        { throw std::runtime_error(msg); }
	void precondition_violation(char const* , char const* msg)  { throw std::runtime_error(msg); }
	void postcondition_violation(char const* , char const* msg) { throw std::runtime_error(msg); }
};
#ifndef __clang__ // currently bugged in clang
template <>
auto ecs::contract_violation_handler<> = unittest_handler{};
#endif

// A helper class that counts invocations of constructers/destructor
struct runtime_ctr_counter {
	inline static int def_ctr_count = 0;
	inline static int ctr_count = 0;
	inline static int copy_count = 0;
	inline static int move_count = 0;
	inline static int dtr_count = 0;

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
		{
			ecs::runtime ecs;
			ecs.add_component({0, 9}, runtime_ctr_counter{});
			ecs.commit_changes();

			CHECK(ecs.get_component_count<runtime_ctr_counter>() == size_t{10});
			CHECK(runtime_ctr_counter::def_ctr_count == 1);
			CHECK(runtime_ctr_counter::move_count == 1);
			CHECK(runtime_ctr_counter::dtr_count == 2);
			CHECK(runtime_ctr_counter::copy_count == 10);
		}
		CHECK(runtime_ctr_counter::dtr_count == 2 + 10);
	}

	SECTION("Allocates storage as needed") {
		ecs::runtime ecs;

		// Use a local struct to avoid it possibly
		// already existing from another unittest
		struct S {
			size_t c;
		};

		// Add a system-less component to an entity
		ecs.add_component(0, S{0});
		ecs.commit_changes();
		CHECK(ecs.get_component_count<S>() == size_t{1});
	}

	SECTION("Supports mutable lambdas") {
		ecs::runtime ecs;
		struct mut_lambda {
			int i;
		};

		// Add some systems to test
		ecs.make_system<ecs::opts::not_parallel>([counter = 0](mut_lambda& ml) mutable { ml.i = counter++; });
		ecs.make_system([](ecs::entity_id ent, mut_lambda const& ml) { CHECK(ent == ml.i); });

		// Create 100 entities and add stuff to them
		ecs.add_component({0, 3}, mut_lambda{0});
		ecs.update();
	}

	SECTION("Ranged add") {
		struct range_add {
			int i;
		};

		SECTION("of components works") {
			ecs::runtime ecs;
			ecs.add_component({0, 5}, range_add{5});
			ecs::entity_range const ents{6, 9};
			ecs.add_component(ents, range_add{5});
			ecs.commit_changes();

			for (ecs::entity_id i = 0; i <= 9; ++i) {
				auto const& ra = *ecs.get_component<range_add>(i);
				CHECK(ra.i == 5);
			}
		}

		SECTION("of span of components works") {
			ecs::runtime ecs;

			std::vector<int> vec(10, 42);
			ecs.add_component_span({0, 9}, vec);

			ecs.commit_changes();
			REQUIRE(10 == ecs.get_component_count<int>());

			for (ecs::entity_id ent = 0; ent <= 9; ++ent) {
				int i = *ecs.get_component<int>(ent);
				CHECK(i == 42);
			}
		}

		SECTION("with a span must be equal in size") {
			ecs::runtime rt;
			
			// 10 ints
			std::array<int, 10> ints;
			std::iota(ints.begin(), ints.end(), 0);

			// 7 entities, must throw
#ifndef __clang__
			REQUIRE_THROWS(rt.add_component_span({0, 6}, ints));
#endif
		}

		SECTION("with a span must be equal in size") {
			ecs::runtime rt;
			
			// 10 ints
			std::array<int, 10> ints;
			std::iota(ints.begin(), ints.end(), 0);

			// 7 entities, must throw
#ifndef __clang__
			REQUIRE_THROWS(rt.add_component_span({0, 6}, ints));
#endif
		}

		SECTION("of components with generator works") {
			ecs::runtime ecs;
			auto const init = [](ecs::entity_id ent) -> range_add { return {ent * 2}; };

			ecs.add_component_generator({0, 5}, init);

			ecs.commit_changes();
			REQUIRE(6 == ecs.get_component_count<range_add>());

			int i = 0;
			for (auto const& ra : ecs.get_components<range_add>({0, 5})) {
				CHECK(ra.i == i * 2);
				i++;
			}
		}
	}
}

#pragma once
#include <type_traits>

namespace ecs
{
	// Inherit to mark a component as an empty tag
	// Uses O(1) memory instead of O(n).
	struct tag { unsigned char __max_size_of_a_tag_is_1; };

	// Inherit to mark a component as transient.
	// The component will only exist on an entity for one cycle.
	struct transient {};

	// Inherit to mark a component as shared.
	// Uses O(1) memory instead of O(n).
	struct shared {};

	namespace detail {
		// Some helpers
		template <class T> constexpr bool is_tagged_v = std::is_base_of_v<ecs::tag, T>;
		template <class T> constexpr bool is_shared_v = std::is_base_of_v<ecs::shared, T>;
		template <class T> constexpr bool is_transient_v = std::is_base_of_v<ecs::transient, T>;
	}
}

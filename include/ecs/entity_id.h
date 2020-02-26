#pragma once
#include <cstdint>
#include <numeric>

namespace ecs
{
	// A simple struct that is an entity identifier.
	// Use a struct so the typesystem can differentiate
	// between entity ids and regular integers in system arguments
	struct entity_id final
	{
		// Uninitialized entity ids are not allowed, because they make no sense
		entity_id() = delete;
		entity_id(std::int32_t _id) noexcept
			: id(_id)
		{ }

		operator std::int32_t& () noexcept { return id; }
		operator std::int32_t () const noexcept { return id; }

	private:
		std::int32_t id;
	};
}

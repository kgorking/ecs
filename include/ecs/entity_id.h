#pragma once
#include <cstdint>
#include <numeric>

namespace ecs
{
	// Use a struct so the typesystem can differentiate
	// between entity ids and regular integers in system arguments
	struct entity_id final
	{
		// Uninitialized entity ids are not allowed, because they make no sense
		entity_id() = delete;
		entity_id(std::int32_t _id)
			: id(_id)
		{ }

		operator std::int32_t& () { return id; }
		operator std::int32_t () const { return id; }

	private:
		std::int32_t id;
	};
}

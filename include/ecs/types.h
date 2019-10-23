#pragma once
#include <cstdint>

namespace ecs
{
	// Use a struct so the typesystem can differentiate
	// between entity ids and regular integers in system arguments
	struct entity_id final
	{
		std::int32_t id;

		entity_id() noexcept
			: id{ std::numeric_limits<std::int32_t>::max() }
		{ }
		entity_id(std::int32_t _id) noexcept
			: id(_id)
		{ }

		//auto operator <=> (entity_id const&) = default;
		bool operator < (entity_id const& other) const noexcept { return id < other.id; }
		bool operator <=(entity_id const& other) const noexcept { return id <= other.id; }
		bool operator ==(entity_id const& other) const noexcept { return id == other.id; }
		bool operator !=(entity_id const& other) const noexcept { return id != other.id; }
		bool operator >=(entity_id const& other) const noexcept { return id >= other.id; }
		bool operator > (entity_id const& other) const noexcept { return id > other.id; }

		entity_id& operator ++() noexcept { ++id; return *this; }
		entity_id& operator --() noexcept { --id; return *this; }
		entity_id operator ++(int) noexcept { auto copy = *this; id++; return copy; }
		entity_id operator --(int) noexcept { auto copy = *this; id--; return copy; }
	};
}

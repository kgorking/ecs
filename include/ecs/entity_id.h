#pragma once
#include <cstdint>
#include <numeric>

namespace ecs
{
	// Use a struct so the typesystem can differentiate
	// between entity ids and regular integers in system arguments
	struct entity_id final
	{
		std::int32_t id;

		// Uninitialized entity ids are not allowed, because it makes no sense
		entity_id() = delete;
		entity_id(std::int32_t _id)
			: id(_id)
		{ }

		//auto operator <=> (entity_id const&) = default;
		bool operator < (entity_id const& other) const { return id < other.id; }
		bool operator <=(entity_id const& other) const { return id <= other.id; }
		bool operator ==(entity_id const& other) const { return id == other.id; }
		bool operator !=(entity_id const& other) const { return id != other.id; }
		bool operator >=(entity_id const& other) const { return id >= other.id; }
		bool operator > (entity_id const& other) const { return id > other.id; }

		entity_id& operator ++() { ++id; return *this; }
		entity_id& operator --() { --id; return *this; }
		entity_id operator ++(int) { auto copy = *this; id++; return copy; }
		entity_id operator --(int) { auto copy = *this; id--; return copy; }
	};
}

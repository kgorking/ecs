#pragma once
#include <cstdint>
#include <utility>

namespace ecs
{
	struct entity_id final
	{
		std::uint32_t id = 0;

		entity_id() = default;
		entity_id(std::uint32_t id) noexcept
			: id(id)
		{ }

		friend bool operator == (entity_id const& a, entity_id const& b) noexcept {
			return a.id == b.id;
		}

		friend bool operator < (entity_id const& a, entity_id const& b) noexcept
		{
			return a.id < b.id;
		}
	};
}
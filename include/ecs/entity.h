#ifndef __ENTITY
#define __ENTITY

#include <concepts>
#include "entity_id.h"

namespace ecs
{
	// A simple helper class for easing the adding and removing of components
	class entity final
	{
		entity_id ent;

	public:
		template <std::copyable ...Components>
		constexpr entity(entity_id ent, Components &&... components)
			: ent(ent)
		{
			add<Components...>(std::forward<Components>(components)...);
		}

		template <std::copyable ...Components>
		constexpr void add(Components &&... components) const
		{
			add_components(ent, std::forward<Components>(components)...);
		}

		template <std::copyable ...Components>
		constexpr void add() const
		{
			add_components(ent, Components{}...);
		}

		template <typename ...Components>
		constexpr void remove() const
		{
			(remove_component<Components>(ent), ...);
		}

		template <typename ...Component>
		[[nodiscard]] constexpr bool has() const
		{
			return (has_component<Component>(ent) && ...);
		}

		template <typename Component>
		[[nodiscard]] constexpr Component& get() const
		{
			return get_component<Component>(ent);
		}

		[[nodiscard]] constexpr entity_id get_id() const
		{
			return ent;
		}
	};
}

#endif // !__ENTITY

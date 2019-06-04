#pragma once
#include "types.h"
#include "runtime.h"

namespace ecs
{
	// A simple helper class for easing the adding and removing of components
	class entity final
	{
		entity_id id;

	public:
		template <typename ...Components>
		explicit entity(entity_id ent, Components &&... components)
			: id(ent.id)
		{
			add<Components...>(std::forward<Components>(components)...);
		}

		friend bool operator <  (entity const& a, entity const& b) noexcept { return a.get_id() < b.get_id(); }
		friend bool operator == (entity const& a, entity const& b) noexcept { return a.get_id() == b.get_id(); }

		entity_id get_id() const noexcept
		{
			return id;
		}

		template <typename ...Components>
		void add(Components &&... components)
		{
			(add_component<Components>(id, std::forward<Components>(components)), ...);
		}

		template <typename ...Component>
		void add()
		{
			(add_component<Component>(id, Component{}), ...);
		}

		template <typename ...Components>
		void remove()
		{
			(remove_component<Components>(id), ...);
		}

		template <typename ...Component>
		bool has() const
		{
			return (has_component<Component>(id) && ...);
		}

		template <typename Component>
		Component& get() const
		{
			return get_component<Component>(id);
		}
	};
}

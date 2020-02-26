#pragma once
#include "entity_id.h"

namespace ecs
{
	// Forward declarations
	template <typename T> void add_component(entity_id id, T val);
	template <typename T> void remove_component(entity_id id);
	template <typename T> bool has_component(entity_id id);
	template <typename T> T* get_component(entity_id id);

	// A simple helper class for easing the adding and removing of components
	class entity final
	{
		entity_id id;

	public:
		template <typename ...Components>
		entity(entity_id ent, Components &&... components)
			: id(ent)
		{
			add<Components...>(std::forward<Components>(components)...);
		}

		[[nodiscard]] entity_id get_id() const
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
		[[nodiscard]] bool has() const
		{
			return (has_component<Component>(id) && ...);
		}

		template <typename Component>
		[[nodiscard]] Component& get() const
		{
			return get_component<Component>(id);
		}
	};
}

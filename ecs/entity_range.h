#pragma once
#include "types.h"
#include "runtime.h"
#include <gsl/span>

namespace ecs
{
	// A simple helper class for easing the adding and removing of components.
	// 'last' is included in the range.
	class entity_range final
	{
		entity_id const first_;
		entity_id const last_;

	public:
		template <typename ...Components>
		entity_range(entity_id first, entity_id last, Components&& ... components)
			: first_(first)
			, last_(last)
		{
			Expects(first.id <= last.id);
			add<Components...>(std::forward<Components>(components)...);
		}

		// Returns the first entity in the range
		entity_id first() const noexcept
		{
			return first_;
		}

		// Returns the last entity in the range
		entity_id last() const noexcept
		{
			return last_;
		}

		// Returns the number of entities in this range
		size_t count() const noexcept
		{
			return (last_.id - first_.id) + 1ull;
		}

		// Returns true if the entity is contained in this range
		bool contains(entity_id const ent) const noexcept
		{
			return ent.id >= first_.id && ent.id <= last_.id;
		}

		template <typename ...Components>
		void add(std::function<Components(entity_id)>&& ... inits)
		{
			(add_component_range<Components>(first_, last_, std::forward<std::function<Components(entity_id)>>(inits)), ...);
		}

		template <typename ...Components>
		void add(Components&& ... components)
		{
			(add_component_range<Components>(first_, last_, std::forward<Components>(components)), ...);
		}

		template <typename ...Components>
		void add()
		{
			(add_component_range<Components>(first_, last_, Components{}), ...);
		}

		template <typename ...Components>
		void remove()
		{
			(remove_component_range<Components>(first_, last_), ...);
		}

		template <typename ...Components>
		bool has() const
		{
			return (has_component_range<Components>(first_, last_) && ...);
		}

		template <typename Component>
		gsl::span<Component> get() const
		{
			return gsl::make_span(&ecs::get_component<Component>(first_), count());
		}
	};
}

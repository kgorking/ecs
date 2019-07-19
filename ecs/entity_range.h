#pragma once
#include <optional>
#include <gsl/span>
#include "types.h"
#include "runtime.h"
#include "function.h"

namespace ecs
{
	// A simple helper class for easing the adding and removing of components.
	// 'last' is included in the range.
	class entity_range final
	{
		entity_id first_{ 0 };
		entity_id last_{ 0 };

	public:
		template <typename ...Components>
		entity_range(entity_id first, entity_id last, Components&& ... components) noexcept
			: first_(first)
			, last_(last)
		{
			Expects(first.id <= last.id);
			add<Components...>(std::forward<Components>(components)...);
		}

		entity_range(entity_range const&) = default;
		entity_range(entity_range &&) = default;
		entity_range& operator = (entity_range const&) = default;
		entity_range& operator = (entity_range &&) = default;

		// For sort
		bool operator <(entity_range const& other) const noexcept
		{
			return first_.id < other.first().id;
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

		// Returns true if the ranges are identical
		bool equals(entity_range const other) const noexcept
		{
			return first_.id == other.first().id && last_.id == other.last().id;
		}

		// Returns true if the entity is contained in this range
		bool contains(entity_id const ent) const noexcept
		{
			return ent.id >= first_.id && ent.id <= last_.id;
		}

		// Returns true if the range is contained in this range
		bool contains(entity_range const range) const noexcept
		{
			return range.first().id >= first_.id && range.last().id <= last_.id;
		}

		// Returns the offset of an entity into this range
		gsl::index offset(entity_id const ent) const noexcept
		{
			return ent.id - first_.id;
		}

		bool can_merge(entity_range const other) const noexcept
		{
			return last_.id + 1 == other.first().id;
		}

		bool overlaps(entity_range const other) const noexcept
		{
			// This range completely covers the other range
			// **************
			//      ++++
			//      ----
			if (other.first().id >= first_.id && other.last().id <= last_.id)
				return true;

			// Other range completely covers this range
			//      ****
			// +++++++++++++++
			//      ----
			if (other.first().id < first_.id && other.last().id > last_.id)
				return true;

			// Partial overlap
			//   **************
			// ++++
			//   --
			if (other.first().id > first_.id && other.last().id < last_.id)
				return true;

			// Partial overlap
			// **************
			//             ++++
			//             --
			if (other.first().id < last_.id && other.last().id > last_.id)
				return true;

			return false;
		}

		// Removes a range from another range.
		// If the range was split by the remove, it returns two ranges.
		// Pre: 'other' must be contained in 'range', but must not be equal to it
		static std::pair<entity_range, std::optional<entity_range>> remove(entity_range const& range, entity_range const& other)
		{
			Expects(range.contains(other));
			Expects(!(range.first().id == other.first().id && range.last().id == other.last().id));

			// Remove from the front
			if (other.first() == range.first()) {
				return {
					entity_range{ other.last().id + 1, range.last() },
					std::nullopt
				};
			}

			// Remove from the back
			if (other.last() == range.last()) {
				return {
					entity_range{ range.first(), other.first().id - 1 },
					std::nullopt
				};
			}

			// Remove from the middle
			return {
				entity_range{ range.first(), other.first().id - 1 },
				entity_range{ other.last().id + 1, range.last() }
			};
		}

		static entity_range merge(entity_range const r1, entity_range const r2)
		{
			Expects(r1.last().id+1 == r2.first().id);
			return entity_range{ r1.first(), r2.last() };
		}

		// Returns the intersection of two ranges
		// Pre: The ranges must overlap, the resulting ranges can not have zero-length
		static entity_range intersect(entity_range const& range, entity_range const& other)
		{
			Expects(range.overlaps(other));
			Expects(!(range.first().id == other.first().id && range.last().id == other.last().id));

			entity_id const first{ std::max(range.first().id, other.first().id) };
			entity_id const last{ std::min(range.last().id, other.last().id) };

			return entity_range{ first, last };
		}

		template <typename ...Fns>
		void add_init(Fns ... inits)
		{
			(add_component_range_init(first_, last_, std::forward<Fns>(inits)), ...);
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

#pragma once
#include <optional>
#include <gsl/span>
#include "types.h"
#include "runtime.h"
#include "function.h"

namespace ecs
{
	// Defines a range of entities.
	// 'last' is included in the range.
	class entity_range final
	{
		entity_id first_{ 0 };
		entity_id last_{ 0 };

	public:
		// Iterator support
		class iterator {
			entity_id ent_;

		public:
			// iterator traits
			using difference_type = long;
			using value_type = entity_id;
			using pointer = const entity_id*;
			using reference = const entity_id &;
			using iterator_category = std::random_access_iterator_tag;

			iterator() : ent_(std::numeric_limits<decltype(entity_id::id)>::max()) {}
			iterator(entity_id ent) : ent_(ent) {}
			iterator& operator++() { ent_.id++; return *this; }
			iterator operator++(int) { iterator retval = *this; ++(*this); return retval; }
			iterator operator+(difference_type diff) const { return { ent_.id + diff }; }
			iterator operator+(iterator in_it) const { return { ent_.id + in_it.ent_.id }; }
			difference_type operator-(difference_type diff) const { return { ent_.id - diff }; }
			difference_type operator-(iterator in_it) const { return { ent_.id - in_it.ent_.id }; }
			bool operator==(iterator other) const { return ent_ == other.ent_; }
			bool operator!=(iterator other) const { return !(*this == other); }
			entity_id operator*() { return ent_; }
		};
		iterator begin() { return { first_ }; }
		iterator end() { return { last_.id + 1 }; }

	public:
		template <typename ...Components>
		entity_range(entity_id first, entity_id last, Components&& ... components)
			: first_(first)
			, last_(last)
		{
			Expects(first <= last);
			add<Components...>(std::forward<Components>(components)...);
		}

		entity_range(entity_range const&) = default;
		entity_range(entity_range &&) = default;
		entity_range& operator = (entity_range const&) = default;
		entity_range& operator = (entity_range &&) = default;
		bool operator == (entity_range const& other) const noexcept {
			return equals(other);
		}

		// For sort
		bool operator <(entity_range const& other) const noexcept
		{
			return first_ < other.first() && last_ < other.last();
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
		int count() const noexcept
		{
			return (last_.id - first_.id) + 1;
		}

		// Returns true if the ranges are identical
		bool equals(entity_range const other) const noexcept
		{
			return first_ == other.first() && last_ == other.last();
		}

		// Returns true if the entity is contained in this range
		bool contains(entity_id const ent) const noexcept
		{
			return ent >= first_ && ent <= last_;
		}

		// Returns true if the range is contained in this range
		bool contains(entity_range const range) const noexcept
		{
			return range.first() >= first_ && range.last() <= last_;
		}

		// Returns the offset of an entity into this range
		gsl::index offset(entity_id const ent) const
		{
			Expects(ent >= first_ && ent <= last_);
			return static_cast<gsl::index>(ent.id) - first_.id;
		}

		bool can_merge(entity_range const other) const noexcept
		{
			return last_.id + 1 == other.first().id;
		}

		bool overlaps(entity_range const other) const noexcept
		{
			// Identical ranges
			if (first_ == other.first_ && last_ == other.last_)
				return true;

			// This range completely covers the other range
			// **************
			//      ++++
			if (other.first() >= first_ && other.last() <= last_)
				return true;

			// Other range completely covers this range
			//      ****
			// +++++++++++++++
			if (other.first() <= first_ && other.last() >= last_)
				return true;

			// Partial overlap
			//   **************
			// ++++
			if (other.first() < first_ && other.last() >= first_)
				return true;

			// Partial overlap
			// **************
			//             ++++
			if (other.first() <= last_ && other.last() > last_)
				return true;

			return false;
		}

		// Removes a range from another range.
		// If the range was split by the remove, it returns two ranges.
		// Pre: 'other' must be contained in 'range', but must not be equal to it
		static std::pair<entity_range, std::optional<entity_range>> remove(entity_range const& range, entity_range const& other)
		{
			Expects(range.contains(other));
			Expects(!(range.first() == other.first() && range.last() == other.last()));

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
			Expects(r1.last().id + 1 == r2.first().id);
			return entity_range{ r1.first(), r2.last() };
		}

		// Returns the intersection of two ranges
		// Pre: The ranges must overlap, the resulting ranges can not have zero-length
		static entity_range intersect(entity_range const& range, entity_range const& other)
		{
			Expects(range.overlaps(other));
			//Expects(!(range.first() == other.first() && range.last() == other.last()));

			entity_id const first{ std::max(range.first(), other.first()) };
			entity_id const last { std::min(range.last(),  other.last()) };
			Expects(last.id - first.id >= 0);

			return entity_range{ first, last };
		}

		template <typename ...Fns>
		void add_init(Fns ... inits)
		{
			(add_component_range_init(*this, std::forward<Fns>(inits)), ...);
		}

		template <typename ...Components>
		void add(Components&& ... components)
		{
			(add_component_range<Components>(*this, std::forward<Components>(components)), ...);
		}

		template <typename ...Components>
		void add()
		{
			(add_component_range<Components>(*this, Components{}), ...);
		}

		template <typename ...Components>
		void remove()
		{
			(remove_component_range<Components>(*this), ...);
		}

		template <typename ...Components>
		bool has() const
		{
			return (has_component_range<Components>(*this) && ...);
		}

		template <typename Component>
		gsl::span<Component> get() const
		{
			return gsl::make_span(&ecs::get_component<Component>(first_), count());
		}
	};
}

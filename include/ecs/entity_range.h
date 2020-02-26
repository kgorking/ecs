#pragma once
#include <optional>
#include <gsl/span>
#include "entity_id.h"

namespace ecs
{
	// Forward decls
	class entity_range;
	template <typename T>  void add_component(entity_range range, T val);
	template <typename T>  void remove_component(entity_range range);
	template <typename T>  bool has_component(entity_range range);
	template <typename T>  T* get_component(entity_id id);

	using entity_range_view = gsl::span<entity_range const>;

	// Defines a range of entities.
	// 'last' is included in the range.
	class entity_range final
	{
		entity_id first_{ 0 };
		entity_id last_{ 0 };

	public:
		// Iterator support
		// TODO harden
		class iterator {
			entity_id ent_;

		public:
			// iterator traits
			using difference_type = std::int32_t;
			using value_type = entity_id;
			using pointer = const entity_id*;
			using reference = const entity_id &;
			using iterator_category = std::random_access_iterator_tag;

			iterator() noexcept : ent_(0) {}
			iterator(entity_id ent) noexcept : ent_(ent) {}
			iterator& operator++() { ent_++; return *this; }
			iterator operator++(int) { iterator const retval = *this; ++(*this); return retval; }
			iterator operator+(difference_type diff) const { return { ent_ + diff }; }
			difference_type operator-(difference_type diff) const { return ent_ - diff; }
			//iterator operator+(iterator in_it) const { return { ent_.id + in_it.ent_.id }; }
			difference_type operator-(iterator in_it) const { return ent_ - in_it.ent_; }
			bool operator==(iterator other) const { return ent_ == other.ent_; }
			bool operator!=(iterator other) const { return !(*this == other); }
			entity_id operator*() { return ent_; }
		};
		[[nodiscard]] iterator begin() const { return { first_ }; }
		[[nodiscard]] iterator end() const { return { last_ + 1 }; }

	public:
		entity_range(entity_id first, entity_id last)
			: first_(first)
			, last_(last)
		{
			Expects(first <= last);
		}

		template <typename ...Components>
		entity_range(entity_id first, entity_id last, Components&& ... components)
			: entity_range(first, last)
		{
			add<Components...>(std::forward<Components>(components)...);
		}

		~entity_range() = default;
		entity_range(entity_range const&) = default;
		entity_range(entity_range &&) = default;
		entity_range& operator = (entity_range const&) = default;
		entity_range& operator = (entity_range &&) = default;
		bool operator == (entity_range const& other) const {
			return equals(other);
		}

		// For sort
		bool operator <(entity_range const& other) const
		{
			return first_ < other.first() && last_ < other.last();
		}

		// Returns the first entity in the range
		[[nodiscard]] entity_id first() const
		{
			return first_;
		}

		// Returns the last entity in the range
		[[nodiscard]] entity_id last() const
		{
			return last_;
		}

		// Returns the number of entities in this range
		[[nodiscard]] size_t count() const
		{
			Expects(last_ >= first_);
			return static_cast<size_t>(last_) - first_ + 1;
		}

		// Returns true if the ranges are identical
		[[nodiscard]] bool equals(entity_range const& other) const
		{
			return first_ == other.first() && last_ == other.last();
		}

		// Returns true if the entity is contained in this range
		[[nodiscard]] bool contains(entity_id const& ent) const
		{
			return ent >= first_ && ent <= last_;
		}

		// Returns true if the range is contained in this range
		[[nodiscard]] bool contains(entity_range const& range) const
		{
			return range.first() >= first_ && range.last() <= last_;
		}

		// Returns the offset of an entity into this range
		[[nodiscard]] gsl::index offset(entity_id const ent) const
		{
			Expects(contains(ent));
			return static_cast<gsl::index>(ent) - first_;
		}

		[[nodiscard]] bool can_merge(entity_range const& other) const
		{
			return last_ + 1 == other.first();
		}

		[[nodiscard]] bool overlaps(entity_range const& other) const
		{
			return first_ <= other.last_ && other.first_ <= last_;
		}

		// Removes a range from another range.
		// If the range was split by the remove, it returns two ranges.
		// Pre: 'other' must be contained in 'range', but must not be equal to it
		[[nodiscard]] static std::pair<entity_range, std::optional<entity_range>> remove(entity_range const& range, entity_range const& other)
		{
			Expects(range.contains(other));
			Expects(!range.equals(other));

			// Remove from the front
			if (other.first() == range.first()) {
				return {
					entity_range{ other.last() + 1, range.last() },
					std::nullopt
				};
			}

			// Remove from the back
			if (other.last() == range.last()) {
				return {
					entity_range{ range.first(), other.first() - 1 },
					std::nullopt
				};
			}

			// Remove from the middle
			return {
				entity_range{ range.first(), other.first() - 1 },
				entity_range{ other.last() + 1, range.last() }
			};
		}

		// Combines two ranges into one
		// Pre: r1 and r2 must be adjacent ranges, r1 < r2
		[[nodiscard]] static entity_range merge(entity_range const& r1, entity_range const& r2)
		{
			Expects(r1.can_merge(r2));
			return entity_range{ r1.first(), r2.last() };
		}

		// Returns the intersection of two ranges
		// Pre: The ranges must overlap, the resulting ranges can not have zero-length
		[[nodiscard]] static entity_range intersect(entity_range const& range, entity_range const& other)
		{
			Expects(range.overlaps(other));

			entity_id const first{ std::max(range.first(), other.first()) };
			entity_id const last { std::min(range.last(),  other.last()) };
			Expects(last - first >= 0);

			return entity_range{ first, last };
		}

		template <typename ...Components>
		void add(Components&& ... components)
		{
			//(add_component<Components>(*this, std::forward<Components>(components)), ...);
			add_components(*this, std::forward<Components>(components)...);
		}

		template <typename ...Components>
		void add()
		{
			//(add_component<Components>(*this, Components{}), ...);
			add_components(*this, Components{}...);
		}

		template <typename ...Components>
		void remove()
		{
			(remove_component<Components>(*this), ...);
		}

		template <typename ...Components>
		[[nodiscard]] bool has() const
		{
			return (has_component<Components>(*this) && ...);
		}

		template <typename Component>
		[[nodiscard]] gsl::span<Component> get() const
		{
			return gsl::make_span(get_component<Component>(first_), count());
		}
	};
}

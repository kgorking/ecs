#ifndef __ENTITY_RANGE
#define __ENTITY_RANGE

#include <limits>
#include <iterator>
#include <span>
#include <optional>
#include <algorithm>

#include "contract.h"
#include "entity_id.h"

namespace ecs {
	// Defines a range of entities.
	// 'last' is included in the range.
	class entity_range final {
		entity_id first_;
		entity_id last_;

	public:
		// Iterator support
		class iterator {
			entity_id ent_{ 0 };
			bool valid{ false };

		public:
			// iterator traits
			using difference_type = entity_type;
			using value_type = entity_id;
			using pointer = const entity_id*;
			using reference = const entity_id&;
			using iterator_category = std::random_access_iterator_tag;

			//iterator() = delete; // no such thing as a 'default' entity
			constexpr iterator() noexcept {};
			constexpr iterator(entity_id ent) noexcept : ent_(ent), valid(true) {}
			constexpr iterator& operator++() { Expects(valid); ent_++; return *this; }
			constexpr iterator operator++(int) { Expects(valid); iterator const retval = *this; ++(*this); return retval; }
			constexpr iterator operator+(difference_type diff) const { Expects(valid); return { ent_ + diff }; }
			constexpr difference_type operator-(difference_type diff) const { Expects(valid); return ent_ - diff; }
			constexpr difference_type operator-(iterator other) const { Expects(valid); Expects(other.valid); return ent_ - other.ent_; }
			constexpr bool operator==(iterator other) const { Expects(valid); Expects(other.valid); return ent_ == other.ent_; }
			constexpr bool operator!=(iterator other) const { Expects(valid); Expects(other.valid); return !(*this == other); }
			constexpr entity_id operator*() { Expects(valid); return ent_; }
		};
		[[nodiscard]] constexpr iterator begin() const { return { first_ }; }
		[[nodiscard]] constexpr iterator end() const { return { last_ + 1 }; }

	public:
		entity_range() = delete; // no such thing as a 'default' range

		constexpr entity_range(entity_id first, entity_id last)
			: first_(first)
			, last_(last) {
			Expects(first <= last);
		}

		// Construct an entity range and add components to them.
		template <typename ...Components>
		entity_range(entity_id first, entity_id last, Components&& ... components)
			: first_(first)
			, last_(last) {
			Expects(first <= last);
			add<Components...>(std::forward<Components>(components)...);
		}

		template <typename ...Components>
		void add(Components&& ... components) const {
			add_components(*this, std::forward<Components>(components)...);
		}

		template <typename ...Components>
		void add() const {
			add_components(*this, Components{}...);
		}

		template <std::copyable ...Components>
		void remove() const {
			(remove_component<Components>(*this), ...);
		}

		template <std::copyable ...Components>
		[[nodiscard]] bool has() const {
			return (has_component<Components>(*this) && ...);
		}

		template <std::copyable Component>
		[[nodiscard]] std::span<Component> get() const {
			return std::span(get_component<Component>(first_), count());
		}

		constexpr bool operator == (entity_range const& other) const {
			return equals(other);
		}

		// For sort
		constexpr bool operator <(entity_range const& other) const {
			return first_ < other.first() && last_ < other.last();
		}

		// Returns the first entity in the range
		[[nodiscard]] constexpr entity_id first() const {
			return first_;
		}

		// Returns the last entity in the range
		[[nodiscard]] constexpr entity_id last() const {
			return last_;
		}

		// Returns the number of entities in this range
		[[nodiscard]] constexpr size_t count() const {
			Expects(last_ >= first_);
			return static_cast<size_t>(last_) - first_ + 1;
		}

		// Returns true if the ranges are identical
		[[nodiscard]] constexpr bool equals(entity_range const& other) const {
			return first_ == other.first() && last_ == other.last();
		}

		// Returns true if the entity is contained in this range
		[[nodiscard]] constexpr bool contains(entity_id const& ent) const {
			return ent >= first_ && ent <= last_;
		}

		// Returns true if the range is contained in this range
		[[nodiscard]] constexpr bool contains(entity_range const& range) const {
			return range.first() >= first_ && range.last() <= last_;
		}

		// Returns the offset of an entity into this range
		// Pre: 'ent' must be in the range
		[[nodiscard]] constexpr entity_offset offset(entity_id const ent) const {
			Expects(contains(ent));
			return static_cast<entity_offset>(ent) - first_;
		}

		[[nodiscard]] constexpr bool can_merge(entity_range const& other) const {
			return last_ + 1 == other.first();
		}

		[[nodiscard]] constexpr bool overlaps(entity_range const& other) const {
			return first_ <= other.last_ && other.first_ <= last_;
		}

		// Removes a range from another range.
		// If the range was split by the remove, it returns two ranges.
		// Pre: 'other' must be contained in 'range', but must not be equal to it
		[[nodiscard]] constexpr static std::pair<entity_range, std::optional<entity_range>> remove(entity_range const& range, entity_range const& other) {
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
		[[nodiscard]] constexpr static entity_range merge(entity_range const& r1, entity_range const& r2) {
			Expects(r1.can_merge(r2));
			return entity_range{ r1.first(), r2.last() };
		}

		// Returns the intersection of two ranges
		// Pre: The ranges must overlap, the resulting ranges can not have zero-length
		[[nodiscard]] constexpr static entity_range intersect(entity_range const& range, entity_range const& other) {
			Expects(range.overlaps(other));

			entity_id const first{ std::max(range.first(), other.first()) };
			entity_id const last{ std::min(range.last(),  other.last()) };
			Expects(last - first >= 0);

			return entity_range{ first, last };
		}
	};

	using entity_range_view = std::span<entity_range const>;
}

#endif // !__ENTITTY_RANGE

#ifndef ECS_ENTITY_RANGE_H
#define ECS_ENTITY_RANGE_H

#include <algorithm>
#include <limits>
#include <optional>
#include <span>

#include "detail/contract.h"
#include "detail/entity_iterator.h"
#include "entity_id.h"

namespace ecs {
// Defines a range of entities.
// 'last' is included in the range.
ECS_EXPORT class entity_range final {
	detail::entity_type first_;
	detail::entity_type last_;

public:
	entity_range() = delete; // no such thing as a 'default' range

	constexpr entity_range(detail::entity_type first, detail::entity_type last) : first_(first), last_(last) {
		Pre(first <= last, "invalid interval; first entity can not be larger than the last entity");
	}

	static constexpr entity_range all() {
		return {std::numeric_limits<detail::entity_type>::min(), std::numeric_limits<detail::entity_type>::max()};
	}

	[[nodiscard]] constexpr detail::entity_iterator begin() const {
		return {first_};
	}

	[[nodiscard]] constexpr detail::entity_iterator end() const {
		return {last_ + 1};
	}

	[[nodiscard]] constexpr bool operator==(entity_range const& other) const {
		return equals(other);
	}

	// For sort
	[[nodiscard]] constexpr bool operator<(entity_range const& other) const {
		return /*first_ < other.first() &&*/ last_ < other.first();
	}

	[[nodiscard]] constexpr bool operator<(entity_id const& id) const {
		return last_ < id;
	}

	// Returns the first entity in the range
	[[nodiscard]] constexpr entity_id first() const {
		return entity_id{first_};
	}

	// Returns the last entity in the range
	[[nodiscard]] constexpr entity_id last() const {
		return entity_id{last_};
	}

	// Returns the number of entities in this range
	[[nodiscard]] constexpr ptrdiff_t count() const {
		return static_cast<ptrdiff_t>(last_ - first_ + 1);
	}

	// Returns the number of entities in this range as unsigned
	[[nodiscard]] constexpr size_t ucount() const {
		return static_cast<size_t>(last_ - first_ + 1);
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

	// Returns true if the ranges are adjacent to each other
	[[nodiscard]] constexpr bool adjacent(entity_range const& range) const {
		return first_ - 1 == range.last() || last_ + 1 == range.first();
	}

	// Returns the offset of an entity into this range
	// Pre: 'ent' must be in the range
	[[nodiscard]] constexpr detail::entity_offset offset(entity_id const ent) const {
		Pre(contains(ent), "entity must exist in the range");
		return static_cast<detail::entity_offset>(ent - first_);
	}

	// Returns the entity id at the specified offset
	// Pre: 'offset' is in the range
	[[nodiscard]] entity_id at(detail::entity_offset const offset) const {
		entity_id const id = static_cast<detail::entity_type>(static_cast<detail::entity_offset>(first()) + offset);
		Pre(id <= last(), "offset is out of bounds of the range");
		return id;
	}

	// Returns true if the two ranges touches each other
	[[nodiscard]] constexpr bool overlaps(entity_range const& other) const {
		return first_ <= other.last_ && other.first_ <= last_;
	}

	// Removes a range from another range.
	// If the range was split by the remove, it returns two ranges.
	// Pre: 'other' must overlap 'range', but must not be equal to it
	[[nodiscard]] constexpr static std::pair<entity_range, std::optional<entity_range>> remove(entity_range const& range,
																							   entity_range const& other) {
		Pre(range.overlaps(other), "the two ranges must overlap");
		Pre(!range.equals(other), "the two ranges can not be equal");

		// Remove from the front
		if (other.first() == range.first()) {
			auto const [min, max] = std::minmax({range.last(), other.last()});
			return {entity_range{min + 1, max}, std::nullopt};
		}

		// Remove from the back
		if (other.last() == range.last()) {
			auto const [min, max] = std::minmax({range.first(), other.first()});
			return {entity_range{min, max - 1}, std::nullopt};
		}

		if (range.contains(other)) {
			// Remove from the middle
			return {entity_range{range.first(), other.first() - 1}, entity_range{other.last() + 1, range.last()}};
		} else {
			// Remove overlaps
			if (range.first() < other.first())
				return {entity_range{range.first(), other.first() - 1}, std::nullopt};
			else
				return {entity_range{other.last() + 1, range.last()}, std::nullopt};
		}
	}

	// Combines two ranges into one
	// Pre: r1 and r2 must be adjacent ranges
	[[nodiscard]] constexpr static entity_range merge(entity_range const& r1, entity_range const& r2) {
		Pre(r1.adjacent(r2), "can not merge two ranges that are not adjacent to each other");
		if (r1 < r2)
			return entity_range{r1.first(), r2.last()};
		else
			return entity_range{r2.first(), r1.last()};
	}

	// Returns the intersection of two ranges
	// Pre: The ranges must overlap
	[[nodiscard]] constexpr static entity_range intersect(entity_range const& range, entity_range const& other) {
		Pre(range.overlaps(other), "ranges must overlap in order to intersect");

		entity_id const first{std::max(range.first(), other.first())};
		entity_id const last{std::min(range.last(), other.last())};

		return entity_range{first, last};
	}

	// Returns a range that overlaps the two ranges
	[[nodiscard]] constexpr static entity_range overlapping(entity_range const& r1, entity_range const& r2) {
		entity_id const first{std::min(r1.first(), r2.first())};
		entity_id const last{std::max(r1.last(), r2.last())};

		return entity_range{first, last};
	}
};

// The view of a collection of ranges
using entity_range_view = std::span<entity_range const>;

} // namespace ecs

#endif // !ECS_ENTITTY_RANGE_H

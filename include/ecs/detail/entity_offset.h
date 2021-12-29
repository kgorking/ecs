#ifndef _ENTITY_OFFSET_H
#define _ENTITY_OFFSET_H

#include "../entity_id.h"
#include "../entity_range.h"
#include "contract.h"
#include <numeric>
#include <vector>

namespace ecs::detail {

class entity_offset_conv {
	entity_range_view ranges;
	std::vector<int> range_offsets;

public:
	entity_offset_conv(entity_range_view _ranges) noexcept : ranges(_ranges) {
		range_offsets.resize(ranges.size());
		std::exclusive_scan(ranges.begin(), ranges.end(), range_offsets.begin(), int{0},
							[](int val, entity_range r) { return val + static_cast<int>(r.count()); });
	}

	bool contains(entity_id ent) const noexcept {
		auto const it = std::lower_bound(ranges.begin(), ranges.end(), ent);
		if (it == ranges.end() || !it->contains(ent))
			return false;
		else
			return true;
	}

	int to_offset(entity_id ent) const noexcept {
		auto const it = std::lower_bound(ranges.begin(), ranges.end(), ent);
		Expects(it != ranges.end() && it->contains(ent)); // Expects the entity to be in the ranges

		auto const offset = static_cast<std::size_t>(std::distance(ranges.begin(), it));
		return range_offsets[offset] + (ent - it->first());
	}

	entity_id from_offset(int offset) const noexcept {
		auto const it = std::upper_bound(range_offsets.begin(), range_offsets.end(), offset);
		auto const dist = std::distance(range_offsets.begin(), it);
		auto const dist_prev = static_cast<std::size_t>(std::max(ptrdiff_t{0}, dist - 1));
		return static_cast<entity_id>(ranges[dist_prev].first() + offset - range_offsets[dist_prev]);
	}
};

} // namespace ecs::detail

#endif // !_ENTITY_OFFSET_H

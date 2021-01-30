#ifndef _ENTITY_OFFSET_H
#define _ENTITY_OFFSET_H

#include <vector>
#include <numeric>
#include "contract.h"
#include "../entity_id.h"
#include "../entity_range.h"

namespace ecs::detail {

class entity_offset_conv {
	entity_range_view ranges;
	std::vector<int> range_offsets;

public:
	entity_offset_conv(entity_range_view ranges) noexcept : ranges(ranges) {
		range_offsets.resize(ranges.size());
		std::exclusive_scan(ranges.begin(), ranges.end(), range_offsets.begin(), int{0}, 
			[](int val, entity_range r) {
				return static_cast<int>(val + r.count());
			}
		);
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

		return range_offsets[std::distance(ranges.begin(), it)] + (ent - it->first());
	}

	entity_id from_offset(int offset) const noexcept {
		auto const it = std::upper_bound(range_offsets.begin(), range_offsets.end(), offset);
		auto const dist = std::distance(range_offsets.begin(), it);
		auto const dist_prev = std::max(ptrdiff_t{0}, dist - 1);
		return static_cast<entity_id>(ranges[dist_prev].first() + offset - range_offsets[dist_prev]);
	}
};

} // namespace ecs::detail

#endif // !_ENTITY_OFFSET_H

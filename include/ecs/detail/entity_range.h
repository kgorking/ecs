#ifndef __DETAIL_ENTITY_RANGE
#define __DETAIL_ENTITY_RANGE

#include "../entity_range.h"

namespace ecs::detail {
    // Find the intersectsions between two sets of ranges
    inline std::vector<entity_range> intersect_ranges(entity_range_view view_a, entity_range_view view_b) {
        std::vector<entity_range> result;

        if (view_a.empty() || view_b.empty()) {
            return result;
        }

        auto it_a = view_a.begin();
        auto it_b = view_b.begin();

        while (it_a != view_a.end() && it_b != view_b.end()) {
            if (it_a->overlaps(*it_b)) {
                result.push_back(entity_range::intersect(*it_a, *it_b));
            }

            if (it_a->last() < it_b->last()) { // range a is inside range b, move to
                                               // the next range in a
                ++it_a;
            } else if (it_b->last() < it_a->last()) { // range b is inside range a,
                                                      // move to the next range in b
                ++it_b;
            } else { // ranges are equal, move to next ones
                ++it_a;
                ++it_b;
            }
        }

        return result;
    }

    // Find the difference between two sets of ranges.
    // Removes ranges in b from a.
    inline std::vector<entity_range> difference_ranges(entity_range_view view_a, entity_range_view view_b) {
        if (view_a.empty())
            return {};
        if (view_b.empty())
            return {view_a.begin(), view_a.end()};

        std::vector<entity_range> result;
        auto it_a = view_a.begin();
        auto it_b = view_b.begin();

        auto const add = [&result](entity_range r) {
            if (!result.empty() && result.back().can_merge(r))
                result.back() = entity_range::merge(result.back(), r);
            else
                result.push_back(r);
        };

        auto range_a = *it_a;
        while (it_a != view_a.end() && it_b != view_b.end()) {
            if (it_b->contains(range_a)) {
                // Range 'a' is contained entirely in range 'b',
                // which means that 'a' will not be added.
                if (++it_a != view_a.end())
                    range_a = *it_a;
            } else if (range_a < *it_b) {
                // The whole 'a' range is before 'b', so add range 'a'
                add(range_a);

                if (++it_a != view_a.end())
                    range_a = *it_a;
            } else if (*it_b < range_a) {
                // The whole 'b' range is before 'a', so move ahead
                ++it_b;
            } else {
                // The two ranges overlap

                auto const res = entity_range::remove(range_a, *it_b);

                if (res.second) {
                    // Range 'a' was split in two by range 'b'. Add the first range and update
                    // range 'a' with the second range
                    add(res.first);
                    range_a = *res.second;

                    if (++it_b == view_b.end())
                        add(range_a);
                } else {
                    // Range 'b' removes some of range 'a'

                    if (range_a.first() >= it_b->first()) {
                        // The result is an endpiece, so update the range
                        range_a = res.first;

                        if (++it_b == view_b.end())
                            add(range_a);
                    } else {
                        // Add the range
                        add(res.first);

                        if (++it_a != view_a.end())
                            range_a = *it_a;
                    }
                }
            }
        }

        // add rest of a
        while (it_a != view_a.end())
            add(*it_a++);

        return result;
    }
} // namespace ecs::detail

#endif // !__DETAIL_ENTITY_RANGE

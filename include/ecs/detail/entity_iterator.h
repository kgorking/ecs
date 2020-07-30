#ifndef __ENTITY_ITERATOR
#define __ENTITY_ITERATOR

#include "contract.h"
#include "entity_id.h"
#include <iterator>
#include <limits>

namespace ecs::detail {

    // Iterator support
    class entity_iterator {
    public:
        // iterator traits
        using difference_type = entity_offset;
        using value_type = entity_type;
        using pointer = const entity_type*;
        using reference = const entity_type&;
        using iterator_category = std::random_access_iterator_tag;

        // entity_iterator() = delete; // no such thing as a 'default' entity
        constexpr entity_iterator() noexcept {};

        constexpr entity_iterator(entity_id ent) noexcept
            : ent_(ent) {
        }

        constexpr entity_iterator& operator++() {
            ent_ = step(ent_, 1);
            return *this;
        }

        constexpr entity_iterator operator++(int) {
            entity_iterator const retval = *this;
            ++(*this);
            return retval;
        }

        constexpr entity_iterator operator+(difference_type diff) const {
            return entity_iterator{step(ent_, diff)};
        }

        // Operator exclusively for GCC. This operator is called by GCC's parallel implementation
        // for some goddamn reason. When has this ever been a thing?
        constexpr value_type operator[](int index) const {
            return step(ent_, index);
        }

        constexpr value_type operator-(entity_iterator other) const {
            return step(ent_, -other.ent_);
        }

        constexpr bool operator==(entity_iterator other) const {
            return ent_ == other.ent_;
        }

        constexpr bool operator!=(entity_iterator other) const {
            return !(*this == other);
        }

        constexpr entity_id operator*() {
            return {ent_};
        }

    protected:
        constexpr entity_type step(entity_type start, entity_offset diff) const {
            // ensures the value wraps instead of causing an overflow
            auto const diff_start = static_cast<entity_offset>(start);
            return static_cast<entity_type>(diff_start + diff);
        }

    private:
        value_type ent_{0};
    };
} // namespace ecs

#endif // !__ENTITY_RANGE

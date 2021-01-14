#ifndef __ENTITY_ID
#define __ENTITY_ID

namespace ecs {
    namespace detail {
        using entity_type = int;
        using entity_offset = unsigned int; // must cover the entire entity_type domain
    } // namespace detail

    // A simple struct that is an entity identifier.
    struct entity_id {
        // Uninitialized entity ids are not allowed, because they make no sense
        entity_id() = delete;

        constexpr entity_id(detail::entity_type _id) noexcept
            : id(_id) {
        }

        constexpr operator detail::entity_type & () noexcept {
            return id;
        }
        constexpr operator detail::entity_type const& () const noexcept {
            return id;
        }

    private:
        detail::entity_type id;
    };
} // namespace ecs

#endif // !__ENTITY_ID

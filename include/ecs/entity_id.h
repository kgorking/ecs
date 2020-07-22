#ifndef __ENTITY_ID
#define __ENTITY_ID

namespace ecs {
    using entity_type = int;
    using entity_offset = unsigned int; // must cover the entire entity_type domain

    // A simple struct that is an entity identifier.
    // Use a struct so the typesystem can differentiate
    // between entity ids and regular integers in system arguments
    struct entity_id final {
        // Uninitialized entity ids are not allowed, because they make no sense
        entity_id() = delete;

        constexpr entity_id(entity_type _id) noexcept
            : id(_id) {
        }

        constexpr operator entity_type&() noexcept {
            return id;
        }
        constexpr operator entity_type() const noexcept {
            return id;
        }

    private:
        entity_type id;
    };
} // namespace ecs

#endif // !__ENTITY_ID

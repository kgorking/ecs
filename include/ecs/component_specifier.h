#ifndef __COMPONENT_SPECIFIER
#define __COMPONENT_SPECIFIER

#include <type_traits>

namespace ecs {
    // Add this in 'ecs_flags()' to mark a component as a tag.
    // Uses O(1) memory instead of O(n).
    // Mutually exclusive with 'share' and 'global'
    struct tag {};

    // Add this in 'ecs_flags()' to mark a component as shared between components,
    // meaning that any entity with a shared component will all point to the same component.
    // Think of it as a static member variable in a regular class.
    // Uses O(1) memory instead of O(n).
    // Mutually exclusive with 'tag' and 'global'
    struct share {};

    // Add this in 'ecs_flags()' to mark a component as transient.
    // The component will only exist on an entity for one cycle,
    // and then be automatically removed.
    // Mutually exclusive with 'global'
    struct transient {};

    // Add this in 'ecs_flags()' to mark a component as constant.
    // A compile-time error will be raised if a system tries to
    // access the component through a non-const reference.
    struct immutable {};

    // Add this is 'ecs_flags()' to mark a component as global.
    // Global components can be referenced from systems without
    // having been added to any entities.
    // Uses O(1) memory instead of O(n).
    // Mutually exclusive with 'tag', 'share', and 'transient'
    struct global {};

// Add flags to a component to change its behaviour and memory usage.
// Example:
// struct my_component {
// 	ecs_flags(ecs::tag, ecs::transient);
// 	// component data
// };
#define ecs_flags(...)                                                                             \
    struct _ecs_flags : __VA_ARGS__ {};

    // Some helpers
    namespace detail {
        template<typename T>
        using flags = typename std::remove_cvref_t<T>::_ecs_flags;

        template<typename T>
        concept tagged = std::is_base_of_v<ecs::tag, flags<T>>;

        template<typename T>
        concept shared = std::is_base_of_v<ecs::share, flags<T>>;

        template<typename T>
        concept transient = std::is_base_of_v<ecs::transient, flags<T>>;

        template<typename T>
        concept immutable = std::is_base_of_v<ecs::immutable, flags<T>>;

        template<typename T>
        concept global = std::is_base_of_v<ecs::global, flags<T>>;

        template<typename T>
        concept local = !global<T>;

        template<typename T>
        concept persistent = !transient<T>;

        template<typename T>
        concept unbound = (shared<T> || tagged<T> ||
                           global<T>); // component is not bound to a specific entity (ie static)
    }                                  // namespace detail
} // namespace ecs

#endif // !__COMPONENT_SPECIFIER

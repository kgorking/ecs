#ifndef __COMPONENT_SPECIFIER
#define __COMPONENT_SPECIFIER

#include <type_traits>

namespace ecs {
    namespace flag {
        // Add this in a component with 'ecs_flags()' to mark it as tag.
        // Uses O(1) memory instead of O(n).
        // Mutually exclusive with 'share' and 'global'
        struct tag{};

        // Add this in a component with 'ecs_flags()' to mark it as shared.
        // Any entity with a shared component will all point to the same component.
        // Think of it as a static member variable in a regular class.
        // Uses O(1) memory instead of O(n).
        // Mutually exclusive with 'tag' and 'global'
        struct share{};

        // Add this in a component with 'ecs_flags()' to mark it as transient.
        // The component will only exist on an entity for one cycle,
        // and then be automatically removed.
        // Mutually exclusive with 'global'
        struct transient{};

        // Add this in a component with 'ecs_flags()' to mark it as constant.
        // A compile-time error will be raised if a system tries to
        // access the component through a non-const reference.
        struct immutable{};

        // Add this in a component with 'ecs_flags()' to mark it as global.
        // Global components can be referenced from systems without
        // having been added to any entities.
        // Uses O(1) memory instead of O(n).
        // Mutually exclusive with 'tag', 'share', and 'transient'
        struct global{};
    };

// Add flags to a component to change its behaviour and memory usage.
// Example:
// struct my_component {
// 	ecs_flags(ecs::flag::tag, ecs::flag::transient);
// 	// component data
// };
#define ecs_flags(...)                                                                                                 \
    struct _ecs_flags : __VA_ARGS__ {};

    namespace detail {
        // Some helpers

        template<typename T>
        using flags = typename std::remove_cvref_t<T>::_ecs_flags;

        template<typename T>
        concept tagged = std::is_base_of_v<ecs::flag::tag, flags<T>>;

        template<typename T>
        concept shared = std::is_base_of_v<ecs::flag::share, flags<T>>;

        template<typename T>
        concept transient = std::is_base_of_v<ecs::flag::transient, flags<T>>;

        template<typename T>
        concept immutable = std::is_base_of_v<ecs::flag::immutable, flags<T>>;

        template<typename T>
        concept global = std::is_base_of_v<ecs::flag::global, flags<T>>;

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

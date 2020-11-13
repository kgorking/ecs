#ifndef __COMPONENT_FLAGS_H
#define __COMPONENT_FLAGS_H

// Add flags to a component to change its behaviour and memory usage.
// Example:
// struct my_component {
// 	 ecs_flags(ecs::flag::tag, ecs::flag::transient);
// };
#define ecs_flags(...) struct _ecs_flags : __VA_ARGS__ {}


namespace ecs::flag {
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
}

#endif // !__COMPONENT_FLAGS_H

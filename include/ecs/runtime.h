#ifndef __RUNTIME
#define __RUNTIME

#include <concepts>
#include <execution>
#include <type_traits>
#include <utility>

#include "detail/component_pool.h"
#include "detail/context.h"
#include "detail/system.h"
#include "detail/verification.h"
#include "entity_id.h"
#include "options.h"

namespace ecs {
    // Add several components to a range of entities. Will not be added until 'commit_changes()' is
    // called. Initializers can be used with the function signature 'T(ecs::entity_id)'
    //   where T is the component type returned by the function.
    // Pre: entity does not already have the component, or have it in queue to be added
    template<typename First, typename... T>
    void add_component(entity_range const range, First&& first_val, T&&... vals) {
        static_assert(detail::unique<First, T...>, "the same component was specified more than once");
        static_assert(!detail::global<First> && (!detail::global<T> && ...), "can not add global components to entities");
        static_assert(!std::is_pointer_v<std::remove_cvref_t<First>> && (!std::is_pointer_v<std::remove_cvref_t<T>> && ...), "can not add pointers to entities; wrap them in a struct");

        auto const adder = []<class Type>(entity_range const range, Type&& val) {
            if constexpr (std::is_invocable_v<Type, entity_id> && !detail::unbound<Type>) {
                // Return type of 'func'
                using ComponentType = decltype(std::declval<Type>()(entity_id{0}));
                static_assert(!std::is_same_v<ComponentType, void>,
                    "Initializer functions must return a component");

                // Add it to the component pool
                detail::component_pool<ComponentType>& pool =
                    detail::_context.get_component_pool<ComponentType>();
                pool.add_init(range, std::forward<Type>(val));
            } else {
                // Add it to the component pool
                if constexpr (detail::is_parent<Type>::value) {
                    auto& pool = detail::_context.get_component_pool<detail::parent_id>();
                    pool.add(range, static_cast<detail::parent_id>(val));
                } else if constexpr (std::is_reference_v<Type>) {
                    using DerefT = std::remove_reference_t<Type>;
                    static_assert(std::copyable<DerefT>, "Type must be copyable");

                    detail::component_pool<DerefT>& pool = detail::_context.get_component_pool<DerefT>();
                    pool.add(range, std::forward<DerefT>(val));
                } else {
                    static_assert(std::copyable<Type>, "Type must be copyable");

                    detail::component_pool<Type>& pool = detail::_context.get_component_pool<Type>();
                    pool.add(range, std::forward<Type>(val));
                }
            }
        };

        adder(range, std::forward<First>(first_val));
        (adder(range, std::forward<T>(vals)), ...);
    }

    // Add several components to an entity. Will not be added until 'commit_changes()' is called.
    // Pre: entity does not already have the component, or have it in queue to be added
    template<typename First, typename... T>
    void add_component(entity_id const id, First&& first_val, T&&... vals) {
        add_component(
            entity_range{id, id}, std::forward<First>(first_val), std::forward<T>(vals)...);
    }

    // Removes a component from a range of entities. Will not be removed until 'commit_changes()' is
    // called. Pre: entity has the component
    template<detail::persistent T>
    void remove_component(entity_range const range, T const& = T{}) {
        static_assert(!detail::global<T>, "can not remove or add global components to entities");

        // Remove the entities from the components pool
        detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
        pool.remove_range(range);
    }

    // Removes a component from an entity. Will not be removed until 'commit_changes()' is called.
    // Pre: entity has the component
    template<typename T>
    void remove_component(entity_id const id, T const& = T{}) {
        remove_component<T>({id, id});
    }

    // Returns a shared component.
    template<detail::shared T>
    T& get_shared_component() {
        return detail::_context.get_component_pool<T>().get_shared_component();
    }

    // Returns a global component.
    template<detail::global T>
    T& get_global_component() {
        return detail::_context.get_component_pool<T>().get_shared_component();
    }

    // Returns the component from an entity, or nullptr if the entity is not found
    template<detail::local T>
    T* get_component(entity_id const id) {
        // Get the component pool
        detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
        return pool.find_component_data(id);
    }

    // Returns the components from an entity range, or an empty span if the entities are not found
    // or does not containg the component.
    // The span might be invalidated after a call to 'ecs::commit_changes()'.
    template<detail::local T>
    std::span<T> get_components(entity_range const range) {
        if (!has_component<T>(range))
            return {};

        // Get the component pool
        detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
        return {pool.find_component_data(range.first()), range.count()};
    }

    // Returns the number of active components for a specific type of components
    template<typename T>
    size_t get_component_count() {
        if (!detail::_context.has_component_pool<T>())
            return 0;

        // Get the component pool
        detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
        return pool.num_components();
    }

    // Returns the number of entities that has the component.
    template<typename T>
    size_t get_entity_count() {
        if (!detail::_context.has_component_pool<T>())
            return 0;

        // Get the component pool
        detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
        return pool.num_entities();
    }

    // Return true if an entity contains the component
    template<typename T>
    bool has_component(entity_id const id) {
        if (!detail::_context.has_component_pool<T>())
            return false;

        detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
        return pool.has_entity(id);
    }

    // Returns true if all entities in a range has the component.
    template<typename T>
    bool has_component(entity_range const range) {
        if (!detail::_context.has_component_pool<T>())
            return false;

        detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
        return pool.has_entity(range);
    }

    // Commits the changes to the entities.
    inline void commit_changes() {
        detail::_context.commit_changes();
    }

    // Calls the 'update' function on all the systems in the order they were added.
    inline void run_systems() {
        detail::_context.run_systems();
    }

    // Commits all changes and calls the 'update' function on all the systems in the order they were
    // added. Same as calling commit_changes() and run_systems().
    inline void update() {
        commit_changes();
        run_systems();
    }

    // Make a new system
    template<typename... Options, detail::lambda UpdateFn, typename SortFn = std::nullptr_t>
    auto& make_system(UpdateFn update_func, SortFn sort_func = nullptr) {
        using opts = std::tuple<Options...>;
        return detail::_context.create_system<opts, UpdateFn, SortFn>(
            update_func, sort_func, &UpdateFn::operator());
    }
} // namespace ecs

#endif // !__RUNTIME

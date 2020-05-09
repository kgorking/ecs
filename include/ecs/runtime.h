#ifndef __RUNTIME
#define __RUNTIME

#include <concepts>
#include <execution>
#include <type_traits>
#include <utility>

#include "component_pool.h"
#include "context.h"
#include "entity_id.h"
#include "system.h"
#include "system_verification.h"

namespace ecs {
    // Add components generated from an initializer function to a range of entities. Will not be
    // added until 'commit_changes()' is called. The initializer function signature must be
    //   T(ecs::entity_id)
    // where T is the component type returned by the function.
    // Pre: entity does not already have the component, or have it in queue to be added
    // template<typename Callable> requires std::invocable<Callable, entity_id>
    template<std::invocable<entity_id> Callable>
    void add_component(entity_range const range, Callable&& func) {
        // Return type of 'func'
        using ComponentType = decltype(std::declval<Callable>()(entity_id{0}));
        static_assert(!std::is_same_v<ComponentType, void>, "Initializer functions must return a component");

        // Add it to the component pool
        detail::component_pool<ComponentType>& pool = detail::_context.get_component_pool<ComponentType>();
        pool.add_init(range, std::forward<Callable>(func));
    }

    // Add a component to a range of entities. Will not be added until 'commit_changes()' is called.
    // Pre: entity does not already have the component, or have it in queue to be added
    template<typename T>
    void add_component(entity_range const range, T&& val) {
        static_assert(!std::is_reference_v<T>, "can not store references; pass a copy instead");
        static_assert(std::copyable<T>, "T must be copyable");

        // Add it to the component pool
        detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
        pool.add(range, std::forward<T>(val));
    }

    // Add a component to an entity. Will not be added until 'commit_changes()' is called.
    // Pre: entity does not already have the component, or have it in queue to be added
    template<typename T>
    void add_component(entity_id const id, T&& val) {
        add_component({id, id}, std::forward<T>(val));
    }

    // Add several components to a range of entities. Will not be added until 'commit_changes()' is
    // called. Pre: entity does not already have the component, or have it in queue to be added
    template<typename... T>
    void add_components(entity_range const range, T&&... vals) {
        static_assert(detail::unique<T...>, "the same component was specified more than once");
        (add_component(range, std::forward<T>(vals)), ...);
    }

    // Add several components to an entity. Will not be added until 'commit_changes()' is called.
    // Pre: entity does not already have the component, or have it in queue to be added
    template<typename... T>
    void add_components(entity_id const id, T&&... vals) {
        static_assert(detail::unique<T...>, "the same component was specified more than once");
        (add_component(id, std::forward<T>(vals)), ...);
    }

    // Removes a component from a range of entities. Will not be removed until 'commit_changes()' is
    // called. Pre: entity has the component
    template<detail::persistent T>
    void remove_component(entity_range const range) {
        // Remove the entities from the components pool
        detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
        pool.remove_range(range);
    }

    // Removes a component from an entity. Will not be removed until 'commit_changes()' is called.
    // Pre: entity has the component
    template<typename T>
    void remove_component(entity_id const id) {
        remove_component<T>({id, id});
    }

    // Returns a shared component. Can be called before a system for it has been added
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

    // Returns the number of active components
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
    inline void commit_changes() { detail::_context.commit_changes(); }

    // Calls the 'update' function on all the systems in the order they were added.
    inline void run_systems() { detail::_context.run_systems(); }

    // Commits all changes and calls the 'update' function on all the systems in the order they were
    // added. Same as calling commit_changes() and run_systems().
    inline void update_systems() {
        commit_changes();
        run_systems();
    }

    // Make a new system
    template<int Group = 0, detail::lambda UpdateFn>
    auto& make_system(UpdateFn update_func) {
        return detail::_context.create_system<Group, std::execution::sequenced_policy, UpdateFn>(
            update_func, &UpdateFn::operator());
    }

    // Make a new system with a sort function attached
    template<int Group = 0, detail::lambda UpdateFn, detail::sorter SortFn>
    auto& make_system(UpdateFn update_func, SortFn sort_func) {
        return detail::_context.create_system<Group, std::execution::sequenced_policy, UpdateFn, SortFn>(
            update_func, sort_func, &UpdateFn::operator());
    }

    // Make a new system. It will process components in parallel.
    template<int Group = 0, detail::lambda UpdateFn>
    auto& make_parallel_system(UpdateFn update_func) {
        return detail::_context.create_system<Group, std::execution::parallel_unsequenced_policy, UpdateFn>(
            update_func, &UpdateFn::operator());
    }

    // Make a new system. It will process components in parallel.
    template<int Group = 0, detail::lambda UpdateFn, detail::sorter SortFn>
    auto& make_parallel_system(UpdateFn update_func, SortFn sort_func) {
        return detail::_context.create_system<Group, std::execution::parallel_unsequenced_policy, UpdateFn, SortFn>(
            update_func, sort_func, &UpdateFn::operator());
    }
} // namespace ecs

#endif // !__RUNTIME

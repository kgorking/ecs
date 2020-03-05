#pragma once
#include <execution>
#include "system_verification.h"
#include "entity_range.h"
#include "component_pool.h"
#include "component_specifier.h"
#include "context.h"

namespace ecs {
	// Adds a component to a range of entities. Will not be added until 'commit_changes()' is called.
	// If T is invokable as 'T(entity_id)' the initializer function is called for each entity,
	// and its return type defines the component type
	// Pre: entity does not already have the component, or have it in queue to be added
	template <typename T>
	void add_component(entity_range const range, T&& val) {
		if constexpr (std::is_invocable_v<T, entity_id>) {
			// Return type of 'init'
			using ComponentType = decltype(std::declval<T>()(entity_id{ 0 }));
			static_assert(!std::is_same_v<ComponentType, void>, "Initializer function must return a component");

			// Add it to the component pool
			detail::component_pool<ComponentType>& pool = detail::_context.get_component_pool<ComponentType>();
			pool.add_init(range, std::forward<T>(val));
		}
		else {
			// Add it to the component pool
			detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
			pool.add(range, std::forward<T>(val));
		}
	}

	// Adds several components to a range of entities. Calls 'add_component' for each component
	template <typename ...T>
	void add_components(entity_range const range, T &&... vals) {
		static_assert(detail::unique<T...>, "the same component was specified more than once");
		(add_component(range, std::forward<T>(vals)), ...);
	}

	// Adds a component to an entity. Will not be added until 'commit_changes()' is called.
	// Pre: entity does not already have the component, or have it in queue to be added
	template <typename T>
	void add_component(entity_id const id, T&& val)	{
		add_component({ id, id }, std::forward<T>(val));
	}

	// Adds several components to an entity. Calls 'add_component' for each component
	template <typename ...T>
	void add_components(entity_id const id, T &&... vals) {
		static_assert(detail::unique<T...>, "the same component was specified more than once");
		(add_component(id, std::forward<T>(vals)), ...);
	}

	// Removes a component from a range of entities. Will not be removed until 'commit_changes()' is called.
	// Pre: entity has the component
	template <detail::persistent T>
	void remove_component(entity_range const range) {
		// Remove the entities from the components pool
		detail::component_pool<T> &pool = detail::_context.get_component_pool<T>();
		pool.remove_range(range);
	}

	// Removes a component from an entity. Will not be removed until 'commit_changes()' is called.
	// Pre: entity has the component
	template <typename T>
	void remove_component(entity_id const id) {
		remove_component<T>({ id, id });
	}

	// Removes all components from an entity
	/*inline void remove_all_components(entity_id const id)
	{
		for (auto const& pool : context::internal::component_pools)
			pool->remove(id);
	}*/

	// Returns a shared component. Can be called before a system for it has been added
	template <detail::shared T>
	T& get_shared_component() {
		// Get the pool
		if (!detail::_context.has_component_pool(typeid(T))) {
			detail::_context.init_component_pools<T>();
		}
		return detail::_context.get_component_pool<T>().get_shared_component();
	}

	// Returns the component from an entity, or nullptr if the entity is not found
	template <typename T>
	T* get_component(entity_id const id)
	{
		// Get the component pool
		detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
		return pool.find_component_data(id);
	}

	// Returns the components from an entity range, or an empty span if the entities are not found
	// or does not containg the component.
	// The span might be invalidated after a call to 'ecs::commit_changes()'.
	template <typename T>
	gsl::span<T> get_components(entity_range const range) {
		if (!has_component<T>(range))
			return {};

		// Get the component pool
		detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
		return gsl::make_span(pool.find_component_data(range.first()), static_cast<ptrdiff_t>(range.count()));
	}

	// Returns the number of active components
	template <typename T>
	size_t get_component_count()
	{
		if (!detail::_context.has_component_pool(typeid(T)))
			return 0;

		// Get the component pool
		detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
		return pool.num_components();
	}

	// Returns the number of entities that has the component.
	template <typename T>
	size_t get_entity_count()
	{
		if (!detail::_context.has_component_pool(typeid(T)))
			return 0;

		// Get the component pool
		detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
		return pool.num_entities();
	}

	// Return true if an entity contains the component
	template <typename T>
	bool has_component(entity_id const id)
	{
		if (!detail::_context.has_component_pool(typeid(T)))
			return false;

		detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
		return pool.has_entity(id);
	}

	// Returns true if all entities in a range has the component.
	template <typename T>
	bool has_component(entity_range const range)
	{
		if (!detail::_context.has_component_pool(typeid(T)))
			return false;

		detail::component_pool<T> &pool = detail::_context.get_component_pool<T>();
		return pool.has_entity(range);
	}

	// Commits the changes to the entities.
	inline void commit_changes()
	{
		detail::_context.commit_changes();
	}

	// Calls the 'update' function on all the systems in the order they were added.
	inline void run_systems()
	{
		detail::_context.run_systems();
	}

	// Commits all changes and calls the 'update' function on all the systems in the order they were added.
	// Same as calling commit_changes() and run_systems().
	inline void update_systems()
	{
		commit_changes();
		run_systems();
	}

	// Make a new system
	template <int Group = 0, detail::lambda UserUpdateFunc>
	auto& make_system(UserUpdateFunc update_func) {
		return detail::_context.create_system<Group, std::execution::sequenced_policy, UserUpdateFunc>(update_func, &UserUpdateFunc::operator());
	}

	// Make a new system. It will process components in parallel.
	template <int Group = 0, detail::lambda UserUpdateFunc>
	auto& make_parallel_system(UserUpdateFunc update_func)
	{
		return detail::_context.create_system<Group, std::execution::parallel_unsequenced_policy, UserUpdateFunc>(update_func, &UserUpdateFunc::operator());
	}
}

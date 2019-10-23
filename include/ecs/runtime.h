#pragma once
#include <execution>

#include "component_specifier.h"
#include "entity_id.h"
#include "component_pool_base.h"
#include "component_pool.h"
#include "entity.h"
#include "entity_range.h"
#include "system.h"
#include "system_inspector.h"
#include "context.h"

namespace ecs {
	// Adds a component to a range of entities. Will not be added until 'commit_changes()' is called.
	// If T is invokable as 'T(entity_id)' the initializer function is called for each entity,
	// and its return type defines the component type
	// Pre: entity does not already have the component, or have it in queue to be added
	template <typename T>
	void add_component(entity_range const range, T val)
	{
		bool constexpr invokable = std::is_invocable_v<T, entity_id>;
		if constexpr (invokable) {
			// Return type of 'init'
			using ComponentType = decltype(val(entity_id{ 0 }));
			static_assert(!std::is_same_v<ComponentType, void>, "Initializer function must have a return value");

			// Add it to the component pool
			detail::component_pool<ComponentType>& pool = context::get_component_pool<ComponentType>();
			pool.add_init(range, val);
		}
		else {
			static_assert(std::is_copy_constructible_v<T>, "A copy-constructor for type T is required for this function to work");

			// Add it to the component pool
			detail::component_pool<T>& pool = context::get_component_pool<T>();
			if constexpr (std::is_move_constructible_v<T>)
				pool.add(range, std::move(val));
			else
				pool.add(range, val);
		}
	}

	// Adds a component to an entity. Will not be added until 'commit_changes()' is called.
	// Pre: entity does not already have the component, or have it in queue to be added
	template <typename T>
	void add_component(entity_id const id, T val)
	{
		add_component({ id, id }, std::move(val));
	}

	// Removes a component from a range of entities. Will not be removed until 'commit_changes()' is called.
	// Pre: entity has the component
	template <typename T>
	void remove_component(entity_range const range)
	{
		static_assert(!detail::is_transient_v<T>, "Don't remove transient components manually; it will be handled by the context");

		// Remove the entities from the components pool
		detail::component_pool<T> &pool = context::get_component_pool<T>();
		pool.remove_range(range);
	}

	// Removes a component from an entity. Will not be removed until 'commit_changes()' is called.
	// Pre: entity has the component
	template <typename T>
	void remove_component(entity_id const id)
	{
		remove_component<T>({ id, id });
	}

	// Removes all components from an entity
	/*inline void remove_all_components(entity_id const id)
	{
		for (auto const& pool : context::internal::component_pools)
			pool->remove(id);
	}*/

	// Returns a shared component. Can be called before a system for it has been added
	template <typename T>
	T* get_shared_component()
	{
		static_assert(detail::is_shared_v<T>, "Component has not been marked as shared. Inherit from ecs::shared to fix this.");

		// Get the pool
		if (!context::has_component_pool(typeid(T)))
			context::init_components<T>();
		return context::get_component_pool<T>().get_shared_component();
	}

	// Returns the component from an entity.
	// Pre: the entity has the component
	template <typename T>
	T& get_component(entity_id const id)
	{
		// Get the component pool
		detail::component_pool<T> const& pool = context::get_component_pool<T>();
		return *pool.find_component_data(id);
	}

	// Returns the number of active components
	template <typename T>
	size_t get_component_count()
	{
		if (!context::has_component_pool(typeid(T)))
			return 0;

		// Get the component pool
		detail::component_pool<T> const& pool = context::get_component_pool<T>();
		return pool.num_components();
	}

	// Returns the number of entities that has the component.
	template <typename T>
	size_t get_entity_count()
	{
		if (!context::has_component_pool(typeid(T)))
			return 0;

		// Get the component pool
		detail::component_pool<T> const& pool = context::get_component_pool<T>();
		return pool.num_entities();
	}

	// Return true if an entity contains the component
	template <typename T>
	bool has_component(entity_id const id)
	{
		if (!context::has_component_pool(typeid(T)))
			return false;

		detail::component_pool<T> const& pool = context::get_component_pool<T>();
		return pool.has_entity(id);
	}

	// Returns true if all entities in a range has the component.
	template <typename T>
	bool has_component(entity_range const range)
	{
		if (!context::has_component_pool(typeid(T)))
			return false;

		detail::component_pool<T> &pool = context::get_component_pool<T>();
		return pool.has_entity(range);
	}

	// Commits the changes to the entities.
	inline void commit_changes()
	{
		// Let the component pools handle pending add/remove requests for components
		for (auto const& pool : context::component_pools)
			pool->process_changes();

		// Let the systems respond to any changes in the component pools
		for (auto const& sys : context::systems)
			sys->process_changes();

		// Reset any dirty flags on pools
		for (auto const& pool : context::component_pools)
			pool->clear_flags();
	}

	// Calls the 'update' function on all the systems in the order they were added.
	inline void run_systems()
	{
		for (auto const& sys : context::systems) {
			sys->update();
		}
	}

	// Commits all changes and calls the 'update' function on all the systems in the order they were added.
	// Same as calling commit_changes() and run_systems().
	inline void update_systems()
	{
		commit_changes();
		run_systems();
	}
}

#include "system_impl.h"

namespace ecs
{
	namespace detail
	{
		// detection idiom
		template <class T, class = void>  struct is_lambda : std::false_type {};
		template <class T>                struct is_lambda<T, std::void_t<decltype(&T::operator ())>> : std::true_type {};
		template <class T> static constexpr bool is_lambda_v = is_lambda<T>::value;

		template <class T>
		using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

		template <typename System>
		void verify_system([[maybe_unused]] System update_func) noexcept
		{
			static_assert(detail::is_lambda_v<System>, "System must be a valid lambda or function object");

			using inspector = detail::system_inspector<System>;

			using first_type = typename inspector::template arg_at<0>;

			// Make sure any entity types are not passed as references or pointers
			if constexpr (std::is_reference_v<first_type>) {
				static_assert(!std::is_same_v<std::decay_t<first_type>, entity_id>, "Entities are only passed by value; remove the &");
				static_assert(!std::is_same_v<std::decay_t<first_type>, entity>, "Entities are only passed by value; remove the &");
			}
			if constexpr (std::is_pointer_v<first_type>) {
				static_assert(!std::is_same_v<std::remove_pointer_t<first_type>, entity_id>, "Entity ids are only passed by value; remove the *");
				static_assert(!std::is_same_v<std::remove_pointer_t<first_type>, entity>, "Entity ids are only passed by value; remove the *");
			}

			bool constexpr has_entity_id = std::is_same_v<first_type, entity_id>;
			bool constexpr has_entity_struct = std::is_same_v<first_type, entity>;
			bool constexpr has_entity = has_entity_id || has_entity_struct;

			//
			// Implement the rules for systems
			static_assert(std::is_same_v<typename inspector::return_type, void>, "Systems can not return values");
			static_assert(inspector::num_args > (has_entity ? 1 : 0), "No component types specified for the system");

			//
			// Implement the rules for components
			static_assert(inspector::has_unique_components(), "A component type was specifed more than once");
			static_assert(inspector::components_passed_by_ref(), "Systems can only take references to components");
		}
	}

	// Add a new system to the context. It will process components in parallel.
	template <typename System>
	// requires Callable<System>
	auto& add_system_parallel(System update_func)
	{
		detail::verify_system(update_func);
		return context::create_system<std::execution::parallel_unsequenced_policy, System>(update_func, &System::operator());
	}

	// Add a new system to the context
	template <typename System>
	// requires Callable<System>
	auto& add_system(System update_func)
	{
		detail::verify_system(update_func);
		return context::create_system<std::execution::sequenced_policy, System>(update_func, &System::operator());
	}
}

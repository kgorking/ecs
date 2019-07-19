#pragma once
#include <execution>
#include <algorithm>
#include <vector>
#include <array>
#include <map>
#include <tuple>

#include "types.h"
#include "component_pool.h"
#include "component_specifier.h"
#include "system.h"
#include "system_inspector.h"

namespace ecs
{
	namespace runtime
	{
		namespace internal
		{
			// The vector containing all the systems
			inline std::vector<std::unique_ptr<system>> systems;

			// The vector containing all the component pools
			inline std::vector<std::unique_ptr<detail::component_pool_base>> component_pools;

			// Maps a component type to its components pool
			inline std::map<type_info const*, detail::component_pool_base*> type_pool_lookup;
		}

		// Resets the runtime state. Removes all systems, empties component pools
		inline void reset()
		{
			internal::systems.clear();
			// internal::component_pools.clear(); // this will cause an exception in get_component_pool() due to the cache
			for (auto & pool : internal::component_pools)
				pool->clear();
		}

		// Returns a reference to a components pool.
		// Pre: a pool has been initialized for the type
		inline detail::component_pool_base& get_component_pool(type_info const& type)
		{
			// Simple thread-safe caching
			static thread_local char const* last_type{};
			static thread_local detail::component_pool_base* last_pool{};

			if (last_type == type.raw_name())
				return *last_pool;

			// Look in the pool for the type
			auto const it = internal::type_pool_lookup.find(&type);
			Expects(it != internal::type_pool_lookup.end());

			last_type = type.raw_name();
			last_pool = it->second;

			assert(last_pool != nullptr);
			return *last_pool;
		}

		// Returns a reference to a components pool. Throws 'invalid_argument' if no pool exists
		template <typename T>
		detail::component_pool<T>& get_component_pool()
		{
			detail::component_pool_base &pool = get_component_pool(typeid(T));
			return dynamic_cast<detail::component_pool<T>&>(pool);
		}

		// Returns true if a pool for the type exists
		inline bool has_component_pool(type_info const& type)
		{
			auto const it = internal::type_pool_lookup.find(&type);
			return (it != internal::type_pool_lookup.end());
		}

		// Create a component pool for a new type
		template <typename Component>
		void create_component_pool()
		{
			static_assert(!std::is_pointer_v<Component>, "Will not store pointers in component pools. Use raw types");

			// Create a new pool if one does not already exist
			auto& type = typeid(Component);
			if (!has_component_pool(type))
			{
				auto pool = std::make_unique<detail::component_pool<Component>>();
				internal::type_pool_lookup.emplace(&type, pool.get());
				internal::component_pools.push_back(std::move(pool));
			}
		}

		// Makes sure all the components has a component pool associated with it
		template <typename ... Components>
		void init_components()
		{
			// Create a pool for each component
			(create_component_pool<Components>(), ...);
		}
	}

	// Adds a component to an entity.
	template <typename T>
	void add_component(entity_id const id, T val)
	{
		add_component_range(id, id, std::move(val));
	}

	// Adds a component to a range of entities. 'last' is included in the range.
	// The initializer function 'init' is called for each entity, its return type is the component type
	template <typename Fn>
	void add_component_range_init(entity_id const first, entity_id const last, Fn init)
	{
		Expects(first.id <= last.id);

		// Add it to the component pool
		using T = decltype(init(0));
		detail::component_pool<T>& pool = runtime::get_component_pool<T>();
		pool.add_range_init(first, last, init);
	}

	// Adds a component to a range of entities. 'last' is included in the range.
	template <typename T>
	void add_component_range(entity_id const first, entity_id const last, T val)
	{
		static_assert(std::is_copy_constructible_v<T>, "A copy-constructor for type T is required for this function to work");
		Expects(first.id <= last.id);

		// Add it to the component pool
		detail::component_pool<T> &pool = runtime::get_component_pool<T>();
		pool.add_range(first, last, std::move(val));
	}

	// Removes a component from an entity.
	// Pre: entity has the component
	template <typename T>
	void remove_component(entity_id const id)
	{
		static_assert(!detail::is_transient_v<T>, "Don't remove transient components manually; it will be handled by the runtime");

		// Remove the entity from the components pool
		detail::component_pool<T> &pool = runtime::get_component_pool<T>();
		pool.remove(id);
	}

	// Removes a component from a range of entities. 'last' is included in the range.
	// Pre: entity has the component
	template <typename T>
	void remove_component_range(entity_id const first, entity_id const last)
	{
		static_assert(!detail::is_transient_v<T>, "Don't remove transient components manually; it will be handled by the runtime");

		// Remove the entities from the components pool
		detail::component_pool<T> &pool = runtime::get_component_pool<T>();
		pool.remove_range(first, last);
	}

	// Returns a shared component. Can be called before a system for it has been added
	template <typename T>
	T* get_shared_component()
	{
		static_assert(detail::is_shared_v<T>, "Component has not been marked as shared. Inherit from ecs::shared to fix this.");

		// Get the pool
		if (!runtime::has_component_pool(typeid(T)))
			runtime::init_components<T>();
		return runtime::get_component_pool<T>().get_shared_component();
	}

	// Returns the component from an entity
	template <typename T>
	T& get_component(entity_id const id)
	{
		// Get the component pool
		detail::component_pool<T> &pool = runtime::get_component_pool<T>();
		return *pool.find_component_data(id);
	}

	// Returns the number of active components
	template <typename T>
	size_t get_component_count()
	{
		// Get the component pool
		detail::component_pool<T> const& pool = runtime::get_component_pool<T>();
		return pool.num_components();
	}

	// Returns the number of entities that has a certain components
	template <typename T>
	size_t get_entity_count()
	{
		// Get the component pool
		detail::component_pool<T> const& pool = runtime::get_component_pool<T>();
		return pool.num_entities();
	}

	// Return true if an entity contains a component
	template <typename T>
	bool has_component(entity_id const id)
	{
		detail::component_pool<T> const& pool = runtime::get_component_pool<T>();
		return pool.has_entity(id);
	}

	// Returns true if all entities in a range has a component. 'last' is included in the range.
	template <typename T>
	bool has_component_range(entity_id const first, entity_id const last)
	{
		detail::component_pool<T> &pool = runtime::get_component_pool<T>();

		for (auto id = first; id <= last; ++id) {
			if (!pool.has_entity(id))
				return false;
		}

		return true;
	}

	// Removes all components from an entity, effectively deleting it
	/*inline void remove_entity(entity_id const id)
	{
		for (auto const& pool : runtime::internal::component_pools)
			pool->remove(id);
	}*/

	// Commits the changes to the component pools. Does not run the systems.
	inline void commit_changes()
	{
		// Let the component pools handle pending add/remove requests for components
		for (auto const& pool : runtime::internal::component_pools)
			pool->process_changes();

		// Let the systems respond to any changes in the component pools
		for (auto const& sys : runtime::internal::systems)
			sys->process_changes();

		// Reset any dirty flags on pools
		for (auto const& pool : runtime::internal::component_pools)
			pool->clear_flags();
	}

	// Calls the 'update' function on all the systems in the order they were added.
	inline void run_systems() noexcept
	{
		for (auto const& sys : runtime::internal::systems) {
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
#include "entity.h"

namespace ecs
{
	namespace detail
	{
		// detection idiom
		template <class T, class = void>  struct is_lambda : std::false_type {};
		template <class T>                struct is_lambda<T, std::void_t<decltype(&T::operator ())>> : std::true_type {};
		template <class T> static constexpr bool is_lambda_v = is_lambda<T>::value;

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
			static_assert(std::is_same_v<inspector::return_type, void>, "Systems can not return values");
			static_assert(inspector::num_args > (has_entity ? 1 : 0), "No component types specified for the system");

			//
			// Implement the rules for components
			static_assert(inspector::has_unique_components(), "A component type was specifed more than once");
			static_assert(inspector::components_passed_by_ref(), "Systems can only take references to components");
		}
	}

	// Add a new system to the runtime. It will process components in parallel.
	template <typename System>
	// requires Callable<System>
	system& add_system_parallel(System update_func)
	{
		detail::verify_system(update_func);
		return runtime::create_system<std::execution::parallel_unsequenced_policy, System>(update_func, &System::operator());
	}

	// Add a new system to the runtime
	template <typename System>
	// requires Callable<System>
	system& add_system(System update_func)
	{
		detail::verify_system(update_func);
		return runtime::create_system<std::execution::sequenced_policy, System>(update_func, &System::operator());
	}

	namespace runtime
	{
		template <typename ExecutionPolicy, typename System, typename R, typename C, typename FirstArg, typename ...Args>
		system& create_system_impl(System update_func)
		{
			// Set up the implementation
			using implementation = ecs::detail::system_impl<ExecutionPolicy, System, std::decay_t<FirstArg>, std::decay_t<Args>...>;

			// Is the first argument an entity of sorts?
			bool constexpr has_entity = std::is_same_v<FirstArg, entity_id> || std::is_same_v<FirstArg, entity>;

			// Set up everything for the component pool
			if constexpr (!has_entity)
				init_components<std::decay_t<FirstArg>>();
			init_components<std::decay_t<Args>...>();

			// Create the system instance
			if constexpr (has_entity)
			{
				auto system = std::make_unique<implementation>(update_func, /* dont add the entity as a component pool */ get_component_pool<std::decay_t<Args>>()...);
				internal::systems.push_back(std::move(system));
				return *internal::systems.back().get();
			}
			else
			{
				auto system = std::make_unique<implementation>(update_func, get_component_pool<std::decay_t<FirstArg>>(), get_component_pool<std::decay_t<Args>>()...);
				internal::systems.push_back(std::move(system));
				return *internal::systems.back().get();
			}
		}

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		system& create_system(System update_func, R(C::*)(Args...) const)
		{
			return create_system_impl<ExecutionPolicy, System, R, C, Args...>(update_func);
		}

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		system& create_system(System update_func, R(C::*)(ecs::entity, Args...) const)
		{
			return create_system_impl<ExecutionPolicy, System, R, C, ecs::entity, Args...>(update_func);
		}

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		system& create_system(System update_func, R(C::*)(ecs::entity_id, Args...) const)
		{
			return create_system_impl<ExecutionPolicy, System, R, C, ecs::entity_id, Args...>(update_func);
		}
	}
}

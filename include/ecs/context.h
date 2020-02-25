#pragma once
#include <vector>
#include <map>
#include <typeindex>
#include <shared_mutex>

#include "component_pool.h"
#include "system_impl.h"

namespace ecs::detail {
	// The central class of the ecs implementation. Maintains the state of the system.
	class context final
	{
		// The values that make up the ecs core.
		std::vector<std::unique_ptr<system>> systems;
		std::vector<std::unique_ptr<component_pool_base>> component_pools;
		std::map<std::type_index, component_pool_base*> type_pool_lookup;

		mutable std::shared_mutex mutex;

	public:
		// Commits the changes to the entities.
		void commit_changes()
		{
			// Prevent other threads from
			//  adding components
			//  registering new component types
			//  adding new systems
			std::shared_lock lock(mutex);

			// Let the component pools handle pending add/remove requests for components
			for (auto const& pool : component_pools) {
				pool->process_changes();
			}

			// Let the systems respond to any changes in the component pools
			for (auto const& sys : systems) {
				sys->process_changes();
			}

			// Reset any dirty flags on pools
			for (auto const& pool : component_pools) {
				pool->clear_flags();
			}
		}

		// Calls the 'update' function on all the systems in the order they were added.
		void run_systems()
		{
			// Prevent other threads from adding new systems
			std::shared_lock lock(mutex);

			for (auto const& sys : systems) {
				sys->update();
			}
		}

		// Returns true if a pool for the type exists
		bool has_component_pool(type_info const& type) const
		{
			// Prevent other threads from registering new component types
			std::shared_lock lock(mutex);

			auto const it = type_pool_lookup.find(std::type_index(type));
			return (it != type_pool_lookup.end());
		}

		// Resets the runtime state. Removes all systems, empties component pools
		void reset()
		{
			std::unique_lock lock(mutex);

			systems.clear();
			// context::component_pools.clear(); // this will cause an exception in get_component_pool() due to the cache
			for (auto& pool : component_pools) {
				pool->clear();
			}
		}

		// Returns a reference to a components pool.
		// If a pool doesn't exist, one will be created.
		template <typename T>
		component_pool<T>& get_component_pool()
		{
			// Simple thread-safe caching, ~15% performance boost in benchmarks
			struct __internal_dummy {};
			thread_local std::type_index last_type{ typeid(__internal_dummy) };		// init to a function-local type
			thread_local component_pool_base* last_pool{};

			auto const type_index = std::type_index(typeid(T));
			if (type_index != last_type) {
				// Look in the pool for the type
				std::shared_lock lock(mutex);
				auto it = type_pool_lookup.find(type_index);

				if (it == type_pool_lookup.end()) {
					// The pool wasn't found so create it.
					// create_component_pool takes a unique lock, so unlock the
					// shared lock during its call
					lock.unlock();
					create_component_pool<T>();
					lock.lock();

					it = type_pool_lookup.find(type_index); 
					assert(it != type_pool_lookup.end());
				}

				last_type = type_index;
				last_pool = it->second;
			}

			assert(last_pool != nullptr);
			return dynamic_cast<component_pool<T>&>(*last_pool);
		}

		// Initialize a component pool for each component, if needed
		template <typename ... Components>
		void init_component_pools()
		{
			// Create a pool for each component
			(create_component_pool<Components>(), ...);
		}

		// Const lambdas
		template <int Group, typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		auto& create_system(System update_func, R(C::*)(Args...) const)
		{
			return create_system_impl<Group, ExecutionPolicy, System, R, Args...>(update_func);
		}

		// Mutable lambdas
		template <int Group, typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		auto& create_system(System update_func, R(C::*)(Args...))
		{
			return create_system_impl<Group, ExecutionPolicy, System, R, Args...>(update_func);
		}

	private:
		template <int Group, typename ExecutionPolicy, typename System, typename R, typename FirstArg, typename ...Args>
		auto& create_system_impl(System update_func)
		{
			// Set up the implementation
			using typed_system_impl = system_impl<Group, ExecutionPolicy, System, std::remove_cv_t<std::remove_reference_t<FirstArg>>, std::remove_cv_t<std::remove_reference_t<Args>>...>;

			// Is the first argument an entity of sorts?
			bool constexpr has_entity = std::is_same_v<FirstArg, entity_id> || std::is_same_v<FirstArg, entity>;

			// Set up everything for the component pool
			if constexpr (!has_entity) {
				init_component_pools< std::remove_cv_t<std::remove_reference_t<FirstArg>>>();
			}
			init_component_pools< std::remove_cv_t<std::remove_reference_t<Args>>...>();

			// Create the system instance
			std::unique_ptr<system> sys;
			if constexpr (has_entity) {
				sys = std::make_unique<typed_system_impl>(
					update_func,
					/* dont add the entity as a component pool */
					&get_component_pool<std::remove_cv_t<std::remove_reference_t<Args>>>()...);
			}
			else {
				sys = std::make_unique<typed_system_impl>(
					update_func,
					&get_component_pool<std::remove_cv_t<std::remove_reference_t<FirstArg>>>(),
					&get_component_pool<std::remove_cv_t<std::remove_reference_t<Args>>>()...);
			}

			std::unique_lock lock(mutex);
			systems.push_back(std::move(sys));
			system * ptr_system = systems.back().get();
			Ensures(ptr_system != nullptr);

			sort_systems_by_group();

			return *ptr_system;
		}

		// Sorts the systems based on their group number.
		// The sort maintians ordering in the individual groups.
		void sort_systems_by_group() {
			std::stable_sort(systems.begin(), systems.end(), [](auto const& l, auto const& r) {
				return l.get()->get_group() < r.get()->get_group();
			});
		}

		// Create a component pool for a new type
		template <typename Component>
		void create_component_pool()
		{
			// Create a new pool if one does not already exist
			auto const& type = typeid(Component);
			if (!has_component_pool(type))
			{
				std::unique_lock lock(mutex);

				auto pool = std::make_unique<component_pool<Component>>();
				type_pool_lookup.emplace(std::type_index(type), pool.get());
				component_pools.push_back(std::move(pool));
			}
		}
	};

	inline context& get_context() {
		static context ctx;
		return ctx;
	}

	// The global context
	static inline context & _context = get_context();
}

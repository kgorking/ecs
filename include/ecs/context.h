#pragma once
#include <vector>
#include <map>
#include <typeindex>

#include "component_pool.h"
#include "system_impl.h"

namespace ecs::detail {
	// The central class of the ecs implementation. Maintains the state of the system.
	// Not thread safe.
	class context final
	{
		// The values that make up the ecs core.
		std::vector<std::unique_ptr<system>> systems;
		std::vector<std::unique_ptr<component_pool_base>> component_pools;
		std::map<std::type_index, component_pool_base*> type_pool_lookup;

	public:
		// Commits the changes to the entities.
		void commit_changes()
		{
			// Let the component pools handle pending add/remove requests for components
			for (auto const& pool : component_pools)
				pool->process_changes();

			// Let the systems respond to any changes in the component pools
			for (auto const& sys : systems)
				sys->process_changes();

			// Reset any dirty flags on pools
			for (auto const& pool : component_pools)
				pool->clear_flags();
		}

		// Calls the 'update' function on all the systems in the order they were added.
		void run_systems() noexcept
		{
			for (auto const& sys : systems) {
				sys->update();
			}
		}

		// Returns true if a pool for the type exists
		bool has_component_pool(type_info const& type) const
		{
			auto const it = type_pool_lookup.find(std::type_index(type));
			return (it != type_pool_lookup.end());
		}

		// Resets the runtime state. Removes all systems, empties component pools
		void reset() noexcept
		{
			systems.clear();
			// context::component_pools.clear(); // this will cause an exception in get_component_pool(type_info) due to the cache
			for (auto& pool : component_pools)
				pool->clear();
		}

		// Returns a reference to a components pool.
		// Pre: a pool has been initialized for the type
		component_pool_base& get_component_pool(type_info const& type) const
		{
			// Simple thread-safe caching, ~15% performance boost in benchmarks
			struct __internal_dummy {};
			thread_local std::type_index last_type{ typeid(__internal_dummy) };		// init to a function-local type
			thread_local component_pool_base* last_pool{};

			auto const type_index = std::type_index(type);
			if (last_type == type_index)
				return *last_pool;

			// Look in the pool for the type
			auto const it = type_pool_lookup.find(type_index);
			Expects(it != type_pool_lookup.end());
			Expects(it->second != nullptr);

			last_type = type_index;
			last_pool = it->second;
			return *last_pool;
		}

		// Returns a reference to a components pool.
		// Pre: a pool has been initialized for the type
		template <typename T>
		component_pool<T>& get_component_pool() const
		{
			component_pool_base& pool = get_component_pool(typeid(T));
			return dynamic_cast<component_pool<T>&>(pool);
		}

		// Initialize a component pool for each component, if needed
		template <typename ... Components>
		void init_component_pools()
		{
			// Create a pool for each component
			(create_component_pool<Components>(), ...);
		}

		// Const lambdas
		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		auto& create_system(System update_func, R(C::*)(Args...) const)
		{
			return create_system_impl<ExecutionPolicy, System, R, C, Args...>(update_func);
		}

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		auto& create_system(System update_func, R(C::*)(entity, Args...) const)
		{
			return create_system_impl<ExecutionPolicy, System, R, C, ecs::entity, Args...>(update_func);
		}

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		auto& create_system(System update_func, R(C::*)(entity_id, Args...) const)
		{
			return create_system_impl<ExecutionPolicy, System, R, C, ecs::entity_id, Args...>(update_func);
		}

		// Mutable lambdas
		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		auto& create_system(System update_func, R(C::*)(Args...))
		{
			return create_system_impl<ExecutionPolicy, System, R, C, Args...>(update_func);
		}

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		auto& create_system(System update_func, R(C::*)(entity, Args...))
		{
			return create_system_impl<ExecutionPolicy, System, R, C, ecs::entity, Args...>(update_func);
		}

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		auto& create_system(System update_func, R(C::*)(entity_id, Args...))
		{
			return create_system_impl<ExecutionPolicy, System, R, C, ecs::entity_id, Args...>(update_func);
		}

	private:
		template <typename ExecutionPolicy, typename System, typename R, typename C, typename FirstArg, typename ...Args>
		auto& create_system_impl(System update_func)
		{
			// Set up the implementation
			using typed_system_impl = system_impl<ExecutionPolicy, System, std::decay_t<FirstArg>, std::decay_t<Args>...>;

			// Is the first argument an entity of sorts?
			bool constexpr has_entity = std::is_same_v<FirstArg, entity_id> || std::is_same_v<FirstArg, entity>;

			// Set up everything for the component pool
			if constexpr (!has_entity)
				init_component_pools<std::decay_t<FirstArg>>();
			init_component_pools<std::decay_t<Args>...>();

			// Create the system instance
			if constexpr (has_entity) {
				systems.push_back(std::make_unique<typed_system_impl>(
					update_func,
					/* dont add the entity as a component pool */
					get_component_pool<std::decay_t<Args>>()...));
			}
			else {
				systems.push_back(std::make_unique<typed_system_impl>(
					update_func,
					get_component_pool<std::decay_t<FirstArg>>(),
					get_component_pool<std::decay_t<Args>>()...));
			}

			auto ptr_system = systems.back().get();
			return *ptr_system;
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
				auto pool = std::make_unique<component_pool<Component>>();
				type_pool_lookup.emplace(std::type_index(type), pool.get());
				component_pools.push_back(std::move(pool));
			}
		}
	};

	inline context& get_context() noexcept {
		static context ctx;
		return ctx;
	}

	// The global context
	static inline context & _context = get_context();
}

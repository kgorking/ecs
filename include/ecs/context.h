#pragma once
#include <vector>
#include <map>
#include <typeindex>
#include <memory>

#include "component_pool.h"
#include "system_impl.h"
#include "entity.h"

namespace ecs {
	// Forward declarations needed by the context class
	void run_systems();
	void commit_changes();

	// The central class of the ecs implementation. Maintains the state of the system.
	class context {
		template<class T> static T& _make_static_member() { static T t; return t; }

		// The 'global' values that make up the ecs core.
		static inline auto& systems = _make_static_member<std::vector<std::unique_ptr<system>>>();
		static inline auto& component_pools = _make_static_member<std::vector<std::unique_ptr<detail::component_pool_base>>>();
		static inline auto& type_pool_lookup = _make_static_member<std::map<std::type_index, detail::component_pool_base*>>();

		// The only two functions that are allowed access to the context data
		friend void commit_changes();
		friend void run_systems();

	public:
		// Returns true if a pool for the type exists
		static bool has_component_pool(type_info const& type)
		{
			auto const it = type_pool_lookup.find(std::type_index(type));
			return (it != type_pool_lookup.end());
		}

		// Resets the runtime state. Removes all systems, empties component pools
		static void reset()
		{
			systems.clear();
			// context::component_pools.clear(); // this will cause an exception in get_component_pool(type_info) due to the cache
			for (auto& pool : component_pools)
				pool->clear();
		}

		// Returns a reference to a components pool.
		// Pre: a pool has been initialized for the type
		static detail::component_pool_base& get_component_pool(type_info const& type)
		{
			// Simple thread-safe caching, ~15% performance boost in benchmarks
			struct __internal_dummy {};
			static thread_local std::type_index last_type{ typeid(__internal_dummy) };		// init to a function-local type
			static thread_local detail::component_pool_base* last_pool{};

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
		static detail::component_pool<T>& get_component_pool()
		{
			detail::component_pool_base& pool = get_component_pool(typeid(T));
			return dynamic_cast<detail::component_pool<T>&>(pool);
		}

		// Create a component pool for a new type
		template <typename Component>
		static void create_component_pool()
		{
			static_assert(!std::is_pointer_v<Component>, "Will not store pointers in component pools. Use raw types");

			// Create a new pool if one does not already exist
			auto& type = typeid(Component);
			if (!has_component_pool(type))
			{
				auto pool = std::make_unique<detail::component_pool<Component>>();
				type_pool_lookup.emplace(std::type_index(type), pool.get());
				component_pools.push_back(std::move(pool));
			}
		}

		// Initialize a component pool for each component, if needed
		template <typename ... Components>
		static void init_components()
		{
			// Create a pool for each component
			(create_component_pool<Components>(), ...);
		}

		template <class T>
		using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename FirstArg, typename ...Args>
		static auto& create_system_impl(System update_func)
		{
			// Set up the implementation
			using typed_system_impl = detail::system_impl<ExecutionPolicy, System, remove_cvref_t<FirstArg>, remove_cvref_t<Args>...>;

			// Is the first argument an entity of sorts?
			bool constexpr has_entity = std::is_same_v<FirstArg, entity_id> || std::is_same_v<FirstArg, entity>;

			// Set up everything for the component pool
			if constexpr (!has_entity)
				init_components<remove_cvref_t<FirstArg>>();
			init_components<remove_cvref_t<Args>...>();

			// Create the system instance
			if constexpr (has_entity)
			{
				auto system = std::make_unique<typed_system_impl>(update_func, /* dont add the entity as a component pool */ get_component_pool<remove_cvref_t<Args>>()...);
				systems.push_back(std::move(system));
				return *static_cast<typed_system_impl*>(systems.back().get());
			}
			else
			{
				auto system = std::make_unique<typed_system_impl>(update_func, get_component_pool<std::decay_t<FirstArg>>(), get_component_pool<remove_cvref_t<Args>>()...);
				systems.push_back(std::move(system));
				return *static_cast<typed_system_impl*>(systems.back().get());
			}
		}

		// Const lambdas
		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		static auto& create_system(System update_func, R(C::*)(Args...) const)
		{
			return create_system_impl<ExecutionPolicy, System, R, C, Args...>(update_func);
		}

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		static auto& create_system(System update_func, R(C::*)(entity, Args...) const)
		{
			return create_system_impl<ExecutionPolicy, System, R, C, ecs::entity, Args...>(update_func);
		}

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		static auto& create_system(System update_func, R(C::*)(entity_id, Args...) const)
		{
			return create_system_impl<ExecutionPolicy, System, R, C, ecs::entity_id, Args...>(update_func);
		}

		// Mutable lambdas
		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		static auto& create_system(System update_func, R(C::*)(Args...))
		{
			return create_system_impl<ExecutionPolicy, System, R, C, Args...>(update_func);
		}

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		static auto& create_system(System update_func, R(C::*)(entity, Args...))
		{
			return create_system_impl<ExecutionPolicy, System, R, C, ecs::entity, Args...>(update_func);
		}

		template <typename ExecutionPolicy, typename System, typename R, typename C, typename ...Args>
		static auto& create_system(System update_func, R(C::*)(entity_id, Args...))
		{
			return create_system_impl<ExecutionPolicy, System, R, C, ecs::entity_id, Args...>(update_func);
		}
	};
}

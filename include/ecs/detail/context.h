#ifndef ECS_CONTEXT_H
#define ECS_CONTEXT_H

#include <map>
#include <memory>
//#include <mutex>
#include <shared_mutex>
#include <vector>
#include <execution>

#include "tls/cache.h"
#include "tls/split.h"

#include "component_pools.h"
#include "scheduler.h"
#include "system.h"
#include "system_global.h"
#include "system_hierachy.h"
#include "system_ranged.h"
#include "system_sorted.h"
#include "type_hash.h"
#include "type_list.h"
#include "variant.h"

namespace ecs::detail {
// The central class of the ecs implementation. Maintains the state of the system.
class context final {
	// The values that make up the ecs core.
	std::vector<std::unique_ptr<system_base>> systems;
	std::vector<std::unique_ptr<component_pool_base>> component_pools;
	std::vector<type_hash> pool_type_hash;
	scheduler sched;

	mutable std::shared_mutex system_mutex;
	mutable std::shared_mutex component_pool_mutex;

	bool commit_in_progress = false;
	bool run_in_progress = false;

	using cache_type = tls::cache<type_hash, component_pool_base*, get_type_hash<void>()>;
	tls::split<cache_type> type_caches;

public:
	~context() {
		reset();
	}

	// Commits the changes to the entities.
	void commit_changes() {
		Pre(!commit_in_progress, "a commit is already in progress");
		Pre(!run_in_progress, "can not commit changes while systems are running");

		// Prevent other threads from
		//  adding components
		//  registering new component types
		//  adding new systems
		std::shared_lock system_lock(system_mutex, std::defer_lock);
		std::unique_lock component_pool_lock(component_pool_mutex, std::defer_lock);
		std::lock(system_lock, component_pool_lock); // lock both without deadlock

		commit_in_progress = true;

		static constexpr auto process_changes = [](auto const& inst) {
			inst->process_changes();
		};

		// Let the component pools handle pending add/remove requests for components
		std::for_each(std::execution::par, component_pools.begin(), component_pools.end(), process_changes);

		// Let the systems respond to any changes in the component pools
		std::for_each(std::execution::par, systems.begin(), systems.end(), process_changes);

		// Reset any dirty flags on pools
		for (auto const& pool : component_pools) {
			pool->clear_flags();
		}

		commit_in_progress = false;
	}

	// Calls the 'update' function on all the systems in the order they were added.
	void run_systems() {
		Pre(!commit_in_progress, "can not run systems while changes are being committed");
		Pre(!run_in_progress, "systems are already running");

		// Prevent other threads from adding new systems during the run
		std::shared_lock system_lock(system_mutex);
		run_in_progress = true;

		// Run all the systems
		sched.run();

		run_in_progress = false;
	}

	// Returns true if a pool for the type exists
	template <typename T>
	bool has_component_pool() const {
		// Prevent other threads from registering new component types
		std::shared_lock component_pool_lock(component_pool_mutex);

		static constexpr auto hash = get_type_hash<T>();
		return std::ranges::find(pool_type_hash, hash) != pool_type_hash.end();
	}

	// Resets the runtime state. Removes all systems, empties component pools
	void reset() {
		Pre(!commit_in_progress, "a commit is already in progress");
		Pre(!run_in_progress, "can not commit changes while systems are running");

		std::unique_lock system_lock(system_mutex, std::defer_lock);
		std::unique_lock component_pool_lock(component_pool_mutex, std::defer_lock);
		std::lock(system_lock, component_pool_lock); // lock both without deadlock

		systems.clear();
		sched.clear();
		pool_type_hash.clear();
		component_pools.clear();
		type_caches.clear();
	}

	// Returns a reference to a components pool.
	// If a pool doesn't exist, one will be created.
	template <typename T>
	auto& get_component_pool() {
		// This assert is here to prevent calls like get_component_pool<T> and get_component_pool<T&>,
		// which will produce the exact same code. It should help a bit with compilation times
		// and prevent the compiler from generating duplicated code.
		static_assert(std::is_same_v<T, std::remove_pointer_t<std::remove_cvref_t<T>>>,
					  "This function only takes naked types, like 'int', and not 'int const&' or 'int*'");

		// Don't call this when a commit is in progress
		Pre(!commit_in_progress, "can not get a component pool while a commit is in progress");

		auto& cache = type_caches.local();

		static constexpr auto hash = get_type_hash<T>();
		auto pool = cache.get_or(hash, [this](type_hash _hash) {
			// A new pool might be created, so take a unique lock
			std::unique_lock component_pool_lock(component_pool_mutex);

			// Look in the pool for the type
			auto const it = std::ranges::find(pool_type_hash, _hash);
			if (it == pool_type_hash.end()) {
				// The pool wasn't found so create it.
				return create_component_pool<T>();
			} else {
				return component_pools[std::distance(pool_type_hash.begin(), it)].get();
			}
		});

		return *static_cast<component_pool<T>*>(pool);
	}

	// Regular function
	template <typename Options, typename UpdateFn, typename SortFn, typename R, typename FirstArg, typename... Args>
	decltype(auto) create_system(UpdateFn update_func, SortFn sort_func, R(FirstArg, Args...)) {
		return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(update_func, sort_func);
	}

	// Const lambda with sort
	template <typename Options, typename UpdateFn, typename SortFn, typename R, typename C, typename FirstArg, typename... Args>
	decltype(auto) create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstArg, Args...) const) {
		return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(update_func, sort_func);
	}

	// Mutable lambda with sort
	template <typename Options, typename UpdateFn, typename SortFn, typename R, typename C, typename FirstArg, typename... Args>
	decltype(auto) create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstArg, Args...)) {
		return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(update_func, sort_func);
	}

private:
	template <impl::TypeList ComponentList>
	auto make_pools() {
		using NakedComponentList = transform_type<ComponentList, naked_component_t>;

		return for_all_types<NakedComponentList>([this]<typename... Types>() {
			return detail::component_pools<NakedComponentList>{
				&this->get_component_pool<Types>()...};
		});
	}

	template <typename Options, typename UpdateFn, typename SortFn, typename FirstComponent, typename... Components>
	decltype(auto) create_system(UpdateFn update_func, SortFn sort_func) {
		Pre(!commit_in_progress, "can not create systems while changes are being committed");
		Pre(!run_in_progress, "can not create systems while systems are running");

		// Is the first component an entity_id?
		static constexpr bool first_is_entity = is_entity<FirstComponent>;

		// The type_list of components
		using component_list = std::conditional_t<first_is_entity, type_list<Components...>, type_list<FirstComponent, Components...>>;
	
		// Find potential parent type
		using parent_type = test_option_type_or<is_parent, component_list, void>;

		// Do some checks on the systems
		static bool constexpr has_sort_func = !std::is_same_v<SortFn, std::nullptr_t>;
		static bool constexpr has_parent = !std::is_same_v<void, parent_type>;
		static bool constexpr is_global_sys = for_all_types<component_list>([]<typename... Types>() {
				return (detail::global<Types> && ...);
			});

		static_assert(!(is_global_sys == has_sort_func && is_global_sys), "Global systems can not be sorted");
		static_assert(!(has_sort_func == has_parent && has_parent == true), "Systems can not both be hierarchial and sorted");

		// Helper-lambda to insert system
		auto const insert_system = [this](auto& system) -> decltype(auto) {
			std::unique_lock system_lock(system_mutex);

			[[maybe_unused]] auto sys_ptr = system.get();
			systems.push_back(std::move(system));

			// -vv-  MSVC shenanigans
			[[maybe_unused]] static bool constexpr request_manual_update = has_option<opts::manual_update, Options>();
			if constexpr (!request_manual_update) {
				detail::system_base* ptr_system = systems.back().get();
				sched.insert(ptr_system);
			} else {
				return (*sys_ptr);
			}
		};

		// Create the system instance
		if constexpr (has_parent) {
			using combined_list = detail::merge_type_lists<component_list, parent_type_list_t<parent_type>>;
			using typed_system = system_hierarchy<Options, UpdateFn, first_is_entity, component_list, combined_list, transform_type<combined_list, naked_component_t>>;
			auto sys = std::make_unique<typed_system>(update_func, make_pools<combined_list>());
			return insert_system(sys);
		} else if constexpr (is_global_sys) {
			using typed_system = system_global<Options, UpdateFn, first_is_entity, component_list, transform_type<component_list, naked_component_t>>;
			auto sys = std::make_unique<typed_system>(update_func, make_pools<component_list>());
			return insert_system(sys);
		} else if constexpr (has_sort_func) {
			using typed_system = system_sorted<Options, UpdateFn, SortFn, first_is_entity, component_list, transform_type<component_list, naked_component_t>>;
			auto sys = std::make_unique<typed_system>(update_func, sort_func, make_pools<component_list>());
			return insert_system(sys);
		} else {
			using typed_system = system_ranged<Options, UpdateFn, first_is_entity, component_list, transform_type<component_list, naked_component_t>>;
			auto sys = std::make_unique<typed_system>(update_func, make_pools<component_list>());
			return insert_system(sys);
		}
	}

	template<typename T, typename V>
	void setup_variant_pool(component_pool<T>& pool, component_pool<V>& variant_pool) {
		pool.add_variant(&variant_pool);
		variant_pool.add_variant(&pool);
		if constexpr (is_variant<V> && !std::same_as<T, V>) {
			setup_variant_pool(pool, get_component_pool<variant_t<V>>());
		}
	}

	// Create a component pool for a new type
	template <typename T>
	component_pool_base* create_component_pool() {
		// Create a new pool
		auto pool = std::make_unique<component_pool<T>>();

		// Set up variants
		if constexpr (ecs::detail::is_variant<T>) {
			setup_variant_pool(*pool.get(), get_component_pool<variant_t<T>>());
		}

		// Store the pool and its type hash
		static constexpr auto hash = get_type_hash<T>();
		pool_type_hash.push_back(hash);
		component_pools.push_back(std::move(pool));

		return component_pools.back().get();
	}
};
} // namespace ecs::detail

#endif // !ECS_CONTEXT_H

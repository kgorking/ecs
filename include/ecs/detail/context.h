#ifndef ECS_CONTEXT
#define ECS_CONTEXT

#include <map>
#include <memory>
#include <mutex>
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

namespace ecs::detail {
// The central class of the ecs implementation. Maintains the state of the system.
class context final {
	// The values that make up the ecs core.
	std::vector<std::unique_ptr<system_base>> systems;
	std::vector<std::unique_ptr<component_pool_base>> component_pools;
	std::map<type_hash, component_pool_base*> type_pool_lookup; // TODO vector
	tls::split<tls::cache<type_hash, component_pool_base*, get_type_hash<void>()>> type_caches;
	scheduler sched;

	mutable std::shared_mutex system_mutex;
	mutable std::shared_mutex component_pool_mutex;

public:
	// Commits the changes to the entities.
	void commit_changes() {
		// Prevent other threads from
		//  adding components
		//  registering new component types
		//  adding new systems
		std::shared_lock system_lock(system_mutex, std::defer_lock);
		std::unique_lock component_pool_lock(component_pool_mutex, std::defer_lock);
		std::lock(system_lock, component_pool_lock); // lock both without deadlock

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
	}

	// Calls the 'update' function on all the systems in the order they were added.
	void run_systems() {
		// Prevent other threads from adding new systems during the run
		std::shared_lock system_lock(system_mutex);

		// Run all the systems
		sched.run();
	}

	// Returns true if a pool for the type exists
	template <typename T>
	bool has_component_pool() const {
		// Prevent other threads from registering new component types
		std::shared_lock component_pool_lock(component_pool_mutex);

		static constexpr auto hash = get_type_hash<T>();
		return type_pool_lookup.contains(hash);
	}

	// Resets the runtime state. Removes all systems, empties component pools
	void reset() {
		std::unique_lock system_lock(system_mutex, std::defer_lock);
		std::unique_lock component_pool_lock(component_pool_mutex, std::defer_lock);
		std::lock(system_lock, component_pool_lock); // lock both without deadlock

		systems.clear();
		sched.clear();
		type_pool_lookup.clear();
		component_pools.clear();
		type_caches.reset();
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

		auto& cache = type_caches.local();

		static constexpr auto hash = get_type_hash<T>();
		auto pool = cache.get_or(hash, [this](type_hash _hash) {
			// A new pool might be created, so take a unique lock
			std::unique_lock component_pool_lock(component_pool_mutex);

			// Look in the pool for the type
			auto const it = type_pool_lookup.find(_hash);
			if (it == type_pool_lookup.end()) {
				// The pool wasn't found so create it.
				return create_component_pool<T>();
			} else {
				return it->second;
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
	template<typename T>
	using stripper = reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<T>>>;

	template <impl::TypeList ComponentList>
	auto make_pools() {
		using stripped_list = transform_type<ComponentList, stripper>;

		return for_all_types<stripped_list>([this]<typename... Types>() {
			return detail::component_pools<stripped_list>{
				&this->get_component_pool<Types>()...};
		});
	}

	template <typename Options, typename UpdateFn, typename SortFn, typename FirstComponent, typename... Components>
	decltype(auto) create_system(UpdateFn update_func, SortFn sort_func) {
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

		// Global systems cannot have a sort function
		static_assert(!(is_global_sys == has_sort_func && is_global_sys), "Global systems can not be sorted");

		static_assert(!(has_sort_func == has_parent && has_parent == true), "Systems can not both be hierarchial and sorted");

		// Helper-lambda to insert system
		auto const insert_system = [this](auto& system) -> decltype(auto) {
			std::unique_lock system_lock(system_mutex);

			[[maybe_unused]] auto sys_ptr = system.get();

			systems.push_back(std::move(system));
			detail::system_base* ptr_system = systems.back().get();
			Ensures(ptr_system != nullptr);

			// -vv-  msvc shenanigans
			[[maybe_unused]] static bool constexpr request_manual_update = has_option<opts::manual_update, Options>();
			if constexpr (!request_manual_update) {
				sched.insert(ptr_system);
			} else {
				return (*sys_ptr);
			}
		};

		// Create the system instance
		if constexpr (has_parent) {
			auto const pools = make_pools<detail::merge_type_lists<component_list, parent_type_list_t<parent_type>>>();
			using typed_system = system_hierarchy<Options, UpdateFn, decltype(pools), first_is_entity, component_list>;
			auto sys = std::make_unique<typed_system>(update_func, pools);
			return insert_system(sys);
		} else if constexpr (is_global_sys) {
			auto const pools = make_pools<component_list>();
			using typed_system = system_global<Options, UpdateFn, decltype(pools), first_is_entity, component_list>;
			auto sys = std::make_unique<typed_system>(update_func, pools);
			return insert_system(sys);
		} else if constexpr (has_sort_func) {
			auto const pools = make_pools<component_list>();
			using typed_system = system_sorted<Options, UpdateFn, SortFn, decltype(pools), first_is_entity, component_list>;
			auto sys = std::make_unique<typed_system>(update_func, sort_func, pools);
			return insert_system(sys);
		} else {
			auto const pools = make_pools<component_list>();
			using typed_system = system_ranged<Options, UpdateFn, decltype(pools), first_is_entity, component_list>;
			auto sys = std::make_unique<typed_system>(update_func, pools);
			return insert_system(sys);
		}
	}

	// Create a component pool for a new type
	template <typename T>
	component_pool_base* create_component_pool() {
		// Create a new pool
		auto pool = std::make_unique<component_pool<T>>();
		static constexpr auto hash = get_type_hash<T>();
		type_pool_lookup.emplace(hash, pool.get());
		component_pools.push_back(std::move(pool));
		return component_pools.back().get();
	}
};
} // namespace ecs::detail

#endif // !ECS_CONTEXT

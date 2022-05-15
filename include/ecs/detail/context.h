#ifndef ECS_CONTEXT
#define ECS_CONTEXT

#include <map>
#include <memory>
#include <shared_mutex>
#include <vector>

#include "tls/cache.h"
#include "tls/split.h"

//#include "component_pool.h"
namespace ecs::detail {
class component_pool_base;

template <typename, typename>
class component_pool;
}

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
	std::map<type_hash, component_pool_base*> type_pool_lookup;
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

		auto constexpr process_changes = [](auto const& inst) {
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

		constexpr auto hash = get_type_hash<T>();
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

		constexpr auto hash = get_type_hash<T>();
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
	auto& create_system(UpdateFn update_func, SortFn sort_func, R(FirstArg, Args...)) {
		return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(update_func, sort_func);
	}

	// Const lambda with sort
	template <typename Options, typename UpdateFn, typename SortFn, typename R, typename C, typename FirstArg, typename... Args>
	auto& create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstArg, Args...) const) {
		return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(update_func, sort_func);
	}

	// Mutable lambda with sort
	template <typename Options, typename UpdateFn, typename SortFn, typename R, typename C, typename FirstComponent, typename... Components>
	auto& create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstComponent, Components...)) {
		return create_system<Options, UpdateFn, SortFn, FirstComponent, Components...>(update_func, sort_func);
	}

private:
	template <typename T, typename... R>
	auto make_tuple_pools() {
		using Tr = reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<T>>>;
		if constexpr (!is_entity<Tr>) {
			std::tuple<pool<Tr>, pool<reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<R>>>>...> t(
				&get_component_pool<Tr>(), &get_component_pool<reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<R>>>>()...);
			return t;
		} else {
			std::tuple<pool<reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<R>>>>...> t(
				&get_component_pool<reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<R>>>>()...);
			return t;
		}
	}

	template <typename BF, typename... B, typename... A>
	static auto tuple_cat_unique(std::tuple<A...> const& a, BF* const bf, B... b) {
		if constexpr ((std::is_same_v<BF* const, A> || ...)) {
			// BF exists in tuple a, so skip it
			(void)bf;
			if constexpr (sizeof...(B) > 0) {
				return tuple_cat_unique(a, b...);
			} else {
				return a;
			}
		} else {
			if constexpr (sizeof...(B) > 0) {
				return tuple_cat_unique(std::tuple_cat(a, std::tuple<BF* const>{bf}), b...);
			} else {
				return std::tuple_cat(a, std::tuple<BF* const>{bf});
			}
		}
	}

	template <typename Options, typename UpdateFn, typename SortFn, typename FirstComponent, typename... Components>
	auto& create_system(UpdateFn update_func, SortFn sort_func) {

		// TODO amend options with FirstComponent == entity_id/meta

		// Find potential parent type
		using parent_type =
			test_option_type_or<is_parent, type_list<FirstComponent, Components...>, void>;

		// Do some checks on the systems
		bool constexpr has_sort_func = !std::is_same_v<SortFn, std::nullptr_t>;
		bool constexpr has_parent = !std::is_same_v<void, parent_type>;
		bool constexpr is_global_sys = detail::global<FirstComponent> && (detail::global<Components> && ...);

		// Global systems cannot have a sort function
		static_assert(!(is_global_sys == has_sort_func && is_global_sys), "Global systems can not be sorted");

		static_assert(!(has_sort_func == has_parent && has_parent == true), "Systems can not both be hierarchial and sorted");

		// Helper-lambda to insert system
		auto const insert_system = [this](auto& system) -> decltype(auto) {
			std::unique_lock system_lock(system_mutex);

			auto sys_ptr = system.get();

			systems.push_back(std::move(system));
			detail::system_base* ptr_system = systems.back().get();
			Ensures(ptr_system != nullptr);

			[[maybe_unused]] bool constexpr request_manual_update = has_option<opts::manual_update, Options>();
			if constexpr (!request_manual_update)
				sched.insert(ptr_system);

			return (*sys_ptr);
		};

		// Create the system instance
		if constexpr (has_parent) {
			// Find the component pools
			auto const all_pools = apply_type<parent_type_list_t<parent_type>>([&]<typename... T>() {
				// The pools for the regular components
				auto const pools = make_tuple_pools<FirstComponent, Components...>();

				// Add the pools for the parents components
				if constexpr (sizeof...(T) > 0) {
					return tuple_cat_unique(pools,
											&get_component_pool<reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<T>>>>()...);
				} else {
					return pools;
				}
			});

			using typed_system = system_hierarchy<Options, UpdateFn, decltype(all_pools), FirstComponent, Components...>;
			auto sys = std::make_unique<typed_system>(update_func, all_pools);
			return insert_system(sys);
		} else if constexpr (is_global_sys) {
			auto const pools = make_tuple_pools<FirstComponent, Components...>();
			using typed_system = system_global<Options, UpdateFn, decltype(pools), FirstComponent, Components...>;
			auto sys = std::make_unique<typed_system>(update_func, pools);
			return insert_system(sys);
		} else if constexpr (has_sort_func) {
			auto const pools = make_tuple_pools<FirstComponent, Components...>();
			using typed_system = system_sorted<Options, UpdateFn, SortFn, decltype(pools), FirstComponent, Components...>;
			auto sys = std::make_unique<typed_system>(update_func, sort_func, pools);
			return insert_system(sys);
		} else {
			auto const pools = make_tuple_pools<FirstComponent, Components...>();
			using typed_system = system_ranged<Options, UpdateFn, decltype(pools), FirstComponent, Components...>;
			auto sys = std::make_unique<typed_system>(update_func, pools);
			return insert_system(sys);
		}
	}

	// Create a component pool for a new type
	template <typename T>
	component_pool_base* create_component_pool() {
		// Create a new pool
		auto pool = std::make_unique<component_pool<T>>();
		constexpr auto hash = get_type_hash<T>();
		type_pool_lookup.emplace(hash, pool.get());
		component_pools.push_back(std::move(pool));
		return component_pools.back().get();
	}
};
} // namespace ecs::detail

#endif // !ECS_CONTEXT

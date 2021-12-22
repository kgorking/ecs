#ifndef ECS_CONTEXT_H
#define ECS_CONTEXT_H

#include <map>
#include <memory>
#include <shared_mutex>
#include <vector>

#include "tls/cache.h"
#include "tls/split.h"

#include "component_pool.h"
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
	using hash_pool_pair = std::pair<type_hash, component_pool_base*>;

	// The values that make up the ecs core.
	std::vector<system_base*> systems;  // std::unique_ptr is not yet constexpr [P2273]
	std::vector<hash_pool_pair> component_pools;
	tls::split<tls::cache<type_hash, component_pool_base*, get_type_hash<void>()>> type_caches;
	scheduler sched;

	mutable std::shared_mutex* system_mutex;
	mutable std::shared_mutex* component_pool_mutex;

public:
	constexpr context() {
		if (!std::is_constant_evaluated()) {
			system_mutex = new std::shared_mutex;
			component_pool_mutex = new std::shared_mutex;
		}
	}

	constexpr ~context() {
		for (system_base* sys : systems)
			delete sys;
		for (auto [_, pool] : component_pools)
			delete pool;

		if (!std::is_constant_evaluated()) {
			delete system_mutex;
			delete component_pool_mutex;
		}
	}

	// Commits the changes to the entities.
	constexpr void commit_changes() {
		if (!std::is_constant_evaluated()) {
			// Prevent other threads from
			//  adding components
			//  registering new component types
			//  adding new systems
			std::shared_lock system_lock(*system_mutex, std::defer_lock);
			std::unique_lock component_pool_lock(*component_pool_mutex, std::defer_lock);
			std::lock(system_lock, component_pool_lock); // lock both without deadlock

			// Let the component pools handle pending add/remove requests for components
			std::for_each(std::execution::par, component_pools.begin(), component_pools.end(),
				[](hash_pool_pair& pair) { pair.second->process_changes();
			});

			// Let the systems respond to any changes in the component pools
			std::for_each(std::execution::par, systems.begin(), systems.end(), [](system_base* sys) { sys->process_changes(); });

			// Reset any dirty flags on pools
			for (auto const [_, pool] : component_pools) {
				pool->clear_flags();
			}
		} else {
			// Let the component pools handle pending add/remove requests for components
			for (auto [_, pool] : component_pools)
				pool->process_changes();

			// Let the systems respond to any changes in the component pools
			for (system_base* sys : systems)
				sys->process_changes();

			// Reset any dirty flags on pools
			for (auto const [_, pool] : component_pools) {
				pool->clear_flags();
			}
		}
	}

	// Calls the 'update' function on all the systems in the order they were added.
	void run_systems() {
		// Prevent other threads from adding new systems during the run
		std::shared_lock system_lock(*system_mutex);

		// Run all the systems
		sched.run();
	}

	// Returns true if a pool for the type exists
	template <typename T>
	constexpr bool has_component_pool() const {
		auto const impl = [this] {
			constexpr auto hash = get_type_hash<T>();
			auto const it = std::ranges::find(component_pools, hash, &hash_pool_pair::first);
			return it != component_pools.end();
		};

		if (!std::is_constant_evaluated()) {
			// Prevent other threads from registering new component types
			std::shared_lock component_pool_lock(*component_pool_mutex);
			return impl();
		} else {
			return impl();
		}
	}

	// Resets the runtime state. Removes all systems, empties component pools
	constexpr void reset() {
		auto const impl = [this] {
			systems.clear();
			sched.clear();
			component_pools.clear();

			type_caches.for_each([](auto& cache) { cache.reset(); });
			// type_caches.clear();  // DON'T! It will remove access to existing thread_local vars,
			// which means they can't be reached and reset
		};

		if (!std::is_constant_evaluated()) {
			std::unique_lock system_lock(*system_mutex, std::defer_lock);
			std::unique_lock component_pool_lock(*component_pool_mutex, std::defer_lock);
			std::lock(system_lock, component_pool_lock); // lock both without deadlock
			impl();
		} else {
			impl();
		}
	}

	// Returns a reference to a components pool.
	// If a pool doesn't exist, one will be created.
	template <typename T>
	constexpr component_pool<T>& get_component_pool() {
		// This assert is here to prevent calls like get_component_pool<T> and get_component_pool<T&>,
		// which will produce the exact same code. It should help a bit with compilation times
		// and prevent the compiler from generating duplicated code.
		static_assert(std::is_same_v<T, std::remove_pointer_t<std::remove_cvref_t<T>>>,
					  "This function only takes naked types, like 'int', and not 'int const&' or 'int*'");

		constexpr auto hash = get_type_hash<T>();

		if (!std::is_constant_evaluated()) {
			component_pool_base* pool = type_caches.local().get_or(hash, [this](type_hash _hash) {
				std::shared_lock component_pool_lock(*component_pool_mutex);

				// Look in the pool for the type
				auto const it = std::ranges::find(component_pools, _hash, &hash_pool_pair::first);
				if (it == component_pools.end()) {
					// The pool wasn't found so create it.
					// create_component_pool takes a unique lock, so unlock the
					// shared lock during its call
					component_pool_lock.unlock();
					return create_component_pool<T>();
				} else {
					return it->second;
				}
			});

			return *static_cast<component_pool<T>*>(pool);
		} else {
			// Look in the pool for the type
			for (auto [pool_hash, pool] : component_pools) {
				if (pool_hash == hash) {
					return *static_cast<component_pool<T>*>(pool);
				}
			}

			// The pool wasn't found so create it.
			component_pool_base* pool = create_component_pool<T>();
			return *static_cast<component_pool<T>*>(pool);
		}
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
	constexpr auto make_tuple_pools() {
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
	static constexpr auto tuple_cat_unique(std::tuple<A...> const& a, BF* const bf, B... b) {
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
		// Find potential parent type
		using parent_type =
			test_option_type_or<is_parent, type_list<std::remove_cvref_t<FirstComponent>, std::remove_cvref_t<Components>...>, void>;

		// Do some checks on the systems
		bool constexpr has_sort_func = !std::is_same_v<SortFn, std::nullptr_t>;
		bool constexpr has_parent = !std::is_same_v<void, parent_type>;
		bool constexpr is_global_sys = detail::global<FirstComponent> && (detail::global<Components> && ...);

		// Global systems cannot have a sort function
		static_assert(!(is_global_sys == has_sort_func && is_global_sys), "Global systems can not be sorted");

		static_assert(!(has_sort_func == has_parent && has_parent == true), "Systems can not both be hierarchial and sorted");

		// Create the system instance
		system_base* sys = nullptr;
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
			sys = new typed_system(update_func, all_pools);
		} else if constexpr (is_global_sys) {
			auto const pools = make_tuple_pools<FirstComponent, Components...>();
			using typed_system = system_global<Options, UpdateFn, decltype(pools), FirstComponent, Components...>;
			sys = new typed_system(update_func, pools);
		} else if constexpr (has_sort_func) {
			auto const pools = make_tuple_pools<FirstComponent, Components...>();
			using typed_system = system_sorted<Options, UpdateFn, SortFn, decltype(pools), FirstComponent, Components...>;
			sys = new typed_system(update_func, sort_func, pools);
		} else {
			auto const pools = make_tuple_pools<FirstComponent, Components...>();
			using typed_system = system_ranged<Options, UpdateFn, decltype(pools), FirstComponent, Components...>;
			sys = new typed_system(update_func, pools);
		}

		std::unique_lock system_lock(*system_mutex);
		sys->process_changes(true);
		systems.push_back(sys);

		bool constexpr request_manual_update = has_option<opts::manual_update, Options>();
		if constexpr (!request_manual_update)
			sched.insert(sys);

		return *sys;
	}

	// Create a component pool for a new type
	template <typename T>
	constexpr component_pool_base* create_component_pool() {
		// Create a new pool if one does not already exist
		if (!has_component_pool<T>()) {
			auto const impl = [this] {
				constexpr auto hash = get_type_hash<T>();
				component_pool<T>* pool = new component_pool<T>();
				component_pools.push_back({hash, pool});
				return pool;
			};

			if (!std::is_constant_evaluated()) {
				std::unique_lock component_pool_lock(*component_pool_mutex);
				return impl();
			} else {
				return impl();
			}
		} else
			return &get_component_pool<T>();
	}
};
} // namespace ecs::detail

#endif // !ECS_CONTEXT_H

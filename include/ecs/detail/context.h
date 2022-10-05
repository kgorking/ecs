#ifndef ECS_CONTEXT_H
#define ECS_CONTEXT_H

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

		return apply_type<stripped_list>([this]<typename... Types>() {
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
		static bool constexpr is_global_sys = apply_type<component_list>([]<typename... Types>() {
				return (detail::global<Types> && ...);
			});

		// Global systems cannot have a sort function
		static_assert(!(is_global_sys == has_sort_func && is_global_sys), "Global systems can not be sorted");

		static_assert(!(has_sort_func == has_parent && has_parent == true), "Systems can not both be hierarchial and sorted");

		// Helper-lambda to insert system
		auto const insert_system = [this](auto& system) -> decltype(auto) {
			std::unique_lock system_lock(*system_mutex);

			[[maybe_unused]] auto sys_ptr = system.get();

			systems.push_back(sys_ptr);
			detail::system_base* ptr_system = systems.back();
			Ensures(ptr_system != nullptr);

			// -vv-  msvc shenanigans
			[[maybe_unused]] bool constexpr request_manual_update = has_option<opts::manual_update, Options>();
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

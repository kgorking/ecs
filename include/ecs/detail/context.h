#ifndef __CONTEXT
#define __CONTEXT

#include <map>
#include <memory>
#include <shared_mutex>
#include <vector>

#include "tls/cache.h"
#include "component_pool.h"
#include "scheduler.h"
#include "system.h"
#include "type_hash.h"

namespace ecs::detail {
    // The central class of the ecs implementation. Maintains the state of the system.
    class context final {
        // The values that make up the ecs core.
        std::vector<std::unique_ptr<system_base>> systems;
        std::vector<std::unique_ptr<component_pool_base>> component_pools;
        std::map<type_hash, component_pool_base*> type_pool_lookup;
        scheduler sched;

        mutable std::shared_mutex mutex;

    public:
        // Commits the changes to the entities.
        void commit_changes() {
            // Prevent other threads from
            //  adding components
            //  registering new component types
            //  adding new systems
            std::shared_lock lock(mutex);

            auto constexpr process_changes = [](auto const& inst) { inst->process_changes(); };

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
            std::shared_lock lock(mutex);

            // Run all the systems
            sched.run();
        }

        // Returns true if a pool for the type exists
        template<typename T>
        bool has_component_pool() const {
            // Prevent other threads from registering new component types
            std::shared_lock lock(mutex);

            constexpr auto hash = get_type_hash<T>();
            return type_pool_lookup.contains(hash);
        }

        // Resets the runtime state. Removes all systems, empties component pools
        void reset() {
            std::unique_lock lock(mutex);

            systems.clear();
            sched = scheduler();
            // context::component_pools.clear(); // this will cause an exception in
            // get_component_pool() due to the cache
            for (auto& pool : component_pools) {
                pool->clear();
            }
        }

        // Returns a reference to a components pool.
        // If a pool doesn't exist, one will be created.
        template<typename T>
        auto& get_component_pool() {
            thread_local tls::cache<type_hash, component_pool_base*, get_type_hash<void>()> cache;

            using NakedType = std::remove_pointer_t<std::remove_cvref_t<T>>;

            constexpr auto hash = get_type_hash<NakedType>();
            auto pool = cache.get_or(hash, [this](type_hash hash) {
                std::shared_lock lock(mutex);

                // Look in the pool for the type
                auto const it = type_pool_lookup.find(hash);
                if (it == type_pool_lookup.end()) {
                    // The pool wasn't found so create it.
                    // create_component_pool takes a unique lock, so unlock the
                    // shared lock during its call
                    lock.unlock();
                    return create_component_pool<NakedType>();
                }
                else {
                    return it->second;
                }
            });

            return *static_cast<component_pool<NakedType>*>(pool);
        }

        // Const lambda with sort
        template<typename Options, typename UpdateFn, typename SortFn, typename R, typename C,
            typename FirstArg, typename... Args>
        auto& create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstArg, Args...) const) {
            return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(
                update_func, sort_func);
        }

        // Mutable lambda with sort
        template<typename Options, typename UpdateFn, typename SortFn, typename R, typename C,
            typename FirstArg, typename... Args>
        auto& create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstArg, Args...)) {
            return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(
                update_func, sort_func);
        }

    private:
        template<typename Options, typename UpdateFn, typename SortFn, typename FirstArg,
            typename... Args>
        auto& create_system(UpdateFn update_func, SortFn sort_func) {
            // Make sure we have a valid sort function
            if constexpr (!std::is_same_v<SortFn, std::nullptr_t>) {
                static_assert(detail::sorter<SortFn>, "Invalid sort-function supplied, should be 'bool(T const&, T const&)'");
            }

            // Set up the implementation
            using typed_system = system<Options, UpdateFn, SortFn, FirstArg, Args...>;

            // Create the system instance
            std::unique_ptr<system_base> sys;
            if constexpr (is_entity<FirstArg>()) {
                sys = std::make_unique<typed_system>(update_func, sort_func, &get_component_pool<Args>()...);
            } else {
                sys = std::make_unique<typed_system>(
                    update_func, sort_func, &get_component_pool<FirstArg>(), &get_component_pool<Args>()...);
            }

            std::unique_lock lock(mutex);
            systems.push_back(std::move(sys));
            system_base* ptr_system = systems.back().get();
            Ensures(ptr_system != nullptr);

            if constexpr (!has_option<opts::manual_update, Options>())
                sched.insert(ptr_system);

            return *ptr_system;
        }

        // Create a component pool for a new type
        template<typename T>
        component_pool_base* create_component_pool() {
            // Create a new pool if one does not already exist
            if (!has_component_pool<T>()) {
                std::unique_lock lock(mutex);

                auto pool = std::make_unique<component_pool<T>>();
                constexpr auto hash = get_type_hash<T>();
                type_pool_lookup.emplace(hash, pool.get());
                component_pools.push_back(std::move(pool));
                return component_pools.back().get();
            } else
                return &get_component_pool<T>();
        }
    };

    inline context& get_context() {
        static context ctx;
        return ctx;
    }

    // The global reference to the context
    static inline context& _context = get_context();
} // namespace ecs::detail

#endif // !__CONTEXT

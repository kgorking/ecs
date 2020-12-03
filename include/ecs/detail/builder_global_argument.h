#ifndef __BUILDER_GLOBAL_ARGUMENT_H_
#define __BUILDER_GLOBAL_ARGUMENT_H_

// !Only to be included by system.h

namespace ecs::detail {
    // Manages arguments for global systems.
    template<class Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    struct builder_global_argument {
        // The execution policy is always serial (only run once per cycle)
        using execution_policy = std::execution::sequenced_policy;

        builder_global_argument(UpdateFn update_func, SortFn /*sort*/, tup_pools<FirstComponent, Components...> pools)
            : pools{pools}
            , update_func{update_func}
            , argument{
                &get_pool<FirstComponent>(pools).get_shared_component(),
                &get_pool<Components>(pools).get_shared_component()...} {
        }

        tup_pools<FirstComponent, Components...> get_pools() const {
            return pools;
        }

        void run() {
            update_func(
                *std::get<std::remove_cvref_t<FirstComponent>*>(argument),
                *std::get<std::remove_cvref_t<Components>*>(argument)...);
        }

        // Convert a set of entities into arguments that can be passed to the system
        void build(entity_range_view /*entities*/) {
            // Does nothing
        }

    private:
        // A tuple of the fully typed component pools used by this system
        tup_pools<FirstComponent, Components...> const pools;

        // The user supplied system
        UpdateFn update_func;

        // The arguments for the system
        using global_argument = std::tuple<std::remove_cvref_t<FirstComponent>*, std::remove_cvref_t<Components>*...>;
        global_argument argument;
    };
} // namespace ecs::detail

#endif // !__BUILDER_GLOBAL_ARGUMENT_H_

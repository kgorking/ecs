#ifndef __BUILDER_RANGED_ARGUMENT_H_
#define __BUILDER_RANGED_ARGUMENT_H_

// !Only to be included by system.h

namespace ecs::detail {
    // Manages arguments using ranges. Very fast linear traversal and minimal storage overhead.
    template<class Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    struct builder_ranged_argument {
        // Determine the execution policy from the options (or lack thereof)
        using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(),
            std::execution::sequenced_policy, std::execution::parallel_policy>;

        builder_ranged_argument(
            UpdateFn update_func, SortFn /*sort*/, pool<FirstComponent> first_pool, pool<Components>... pools)
            : pools{first_pool, pools...}
            , update_func{update_func} {
        }

        builder_ranged_argument(UpdateFn update_func, SortFn /*sort*/, pool<Components>... pools)
            : pools{pools...}
            , update_func{update_func} {
        }

        tup_pools<FirstComponent, Components...> get_pools() const {
            return pools;
        }

        void run() {
            // Call the system for all the components that match the system signature
            for (auto const& argument : arguments) {
                auto const& range = std::get<entity_range>(argument);
                auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
                std::for_each(e_p, range.begin(), range.end(), [this, &argument, first_id = range.first()](auto ent) {
                    auto const offset = ent - first_id;
                    if constexpr (is_entity<FirstComponent>) {
                        update_func(ent, extract_arg<Components>(argument, offset)...);
                    } else {
                        update_func(extract_arg<FirstComponent>(argument, offset),
                            extract_arg<Components>(argument, offset)...);
                    }
                });
            }
        }

        // Convert a set of entities into arguments that can be passed to the system
        void build(entity_range_view entities) {
            // Build the arguments for the ranges
            arguments.clear();
            for (auto const& range : entities) {
                if constexpr (is_entity<FirstComponent>) {
                    arguments.emplace_back(range, get_component<Components>(range.first(), pools)...);
                } else {
                    arguments.emplace_back(range, get_component<FirstComponent>(range.first(), pools),
                        get_component<Components>(range.first(), pools)...);
                }
            }
        }

    private:
        // Holds an entity range and its arguments
        using range_argument =
            decltype(std::tuple_cat(std::tuple<entity_range>{{0, 1}}, argument_tuple<FirstComponent, Components...>{}));

        // A tuple of the fully typed component pools used by this system
        tup_pools<FirstComponent, Components...> const pools;

        // The user supplied system
        UpdateFn update_func;

        // Holds the arguments for a range of entities
        std::vector<range_argument> arguments;
    };
}

#endif // !__RANGED_ARGUMENT_BUILDER_H_

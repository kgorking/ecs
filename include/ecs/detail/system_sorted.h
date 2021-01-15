#ifndef __SYSTEM_SORTED_H_
#define __SYSTEM_SORTED_H_

#include "system.h"

namespace ecs::detail {
    // Manages sorted arguments. Neither cache- nor storage space friendly, but arguments
    // will be passed to the user supplied lambda in a sorted manner
    template<typename Options, typename UpdateFn, typename SortFunc, class TupPools, class FirstComponent,
        class... Components>
    struct system_sorted final : public system<Options, UpdateFn, TupPools, FirstComponent, Components...> {
        // Determine the execution policy from the options (or lack thereof)
        using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(),
            std::execution::sequenced_policy, std::execution::parallel_policy>;

    public:
        system_sorted(UpdateFn update_func, SortFunc sort, TupPools pools)
            : system<Options, UpdateFn, TupPools, FirstComponent, Components...>(update_func, pools)
            , sort_func{sort} {
        }

    private:
        void do_run() override {
            // Sort the arguments if the component data has been modified
            if (needs_sorting || std::get<pool<sort_types>>(this->pools)->has_components_been_modified()) {
                auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
                std::sort(e_p, arguments.begin(), arguments.end(), [this](auto const& l, auto const& r) {
                    sort_types* t_l = std::get<sort_types*>(l);
                    sort_types* t_r = std::get<sort_types*>(r);
                    return sort_func(*t_l, *t_r);
                });

                needs_sorting = false;
            }

            auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
            std::for_each(e_p, arguments.begin(), arguments.end(), [this](auto packed_arg) {
                if constexpr (is_entity<FirstComponent>) {
                    this->update_func(std::get<0>(packed_arg), extract_arg<Components>(packed_arg, 0)...);
                } else {
                    this->update_func(
                        extract_arg<FirstComponent>(packed_arg, 0), extract_arg<Components>(packed_arg, 0)...);
                }
            });
        }

        // Convert a set of entities into arguments that can be passed to the system
        void do_build(entity_range_view entities) override {
            if (entities.size() == 0) {
                arguments.clear();
                return;
            }

            // Count the total number of arguments
            size_t arg_count = 0;
            for (auto const& range : entities) {
                arg_count += range.count();
            }

            // Reserve space for the arguments
            arguments.clear();
            arguments.reserve(arg_count);

            // Build the arguments for the ranges
            for (auto const& range : entities) {
                for (entity_id const& entity : range) {
                    if constexpr (is_entity<FirstComponent>) {
                        arguments.emplace_back(entity, get_component<Components>(entity, this->pools)...);
                    } else {
                        arguments.emplace_back(entity, get_component<FirstComponent>(entity, this->pools),
                            get_component<Components>(entity, this->pools)...);
                    }
                }
            }

            needs_sorting = true;
        }

    private:
        // The user supplied sorting function
        SortFunc sort_func;

        // The vector of unrolled arguments, sorted using 'sort_func'
		using argument = single_argument<FirstComponent, Components...>;
		std::vector<argument> arguments;

        // True if the data needs to be sorted
        bool needs_sorting = false;

        using sort_types = typename decltype(get_sorter_types(SortFunc{}))::type1;
    };
} // namespace ecs::detail

#endif // !__SYSTEM_SORTED_H_

#ifndef __BUILDER_HIERARCHY_ARGUMENT_H_
#define __BUILDER_HIERARCHY_ARGUMENT_H_

// !Only to be included by system.h
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "find_entity_pool_intersections.h"
#include "../parent.h"

namespace ecs::detail {
    template<int Index, class Tuple>
    constexpr int count_ptrs_in_tuple() {
        if constexpr (Index == std::tuple_size_v<Tuple>) {
            return 0;
        } else if constexpr (std::is_pointer_v<std::tuple_element_t<Index, Tuple>>) {
            return 1 + count_ptrs_in_tuple<Index + 1, Tuple>();
        } else {
            return count_ptrs_in_tuple<Index + 1, Tuple>();
        }
    }

    template<typename Options, typename UpdateFn, typename SortFn, typename TuplePools, class FirstComponent,
        class... Components>
    class builder_hierarchy_argument {
        // Walking the tree in parallel doesn't seem possible
        using execution_policy = std::execution::sequenced_policy;


        // Extract the parent type
        static constexpr int ParentIndex = test_option_index<is_parent,
            std::tuple<std::remove_cvref_t<FirstComponent>, std::remove_cvref_t<Components>...>>;
        static_assert(-1 != ParentIndex, "no parent component found");

        using full_parent_type = std::tuple_element_t<ParentIndex, std::tuple<FirstComponent, Components...>>;
        using parent_type = std::remove_cvref_t<full_parent_type>;

        // If there is one-or-more sub-components then the parent
        // must be passed as a reference
        static constexpr size_t num_parent_subtype_filters =
            count_ptrs_in_tuple<0, parent_types_tuple_t<parent_type>>();
        static constexpr size_t num_parent_subtypes =
            std::tuple_size_v<parent_types_tuple_t<parent_type>> - num_parent_subtype_filters;
        static_assert((num_parent_subtypes > 0) ? std::is_reference_v<full_parent_type> : true,
            "parents with non-filter sub-components must be passed as references");


        // Holds a single entity id and its arguments
        using single_argument =
            decltype(std::tuple_cat(std::tuple<entity_id>{0}, std::declval<argument_tuple<FirstComponent, Components...>>()));

        using relation_map = std::unordered_map<entity_type, single_argument const*>;
        using relation_mmap = std::multimap<entity_type, single_argument const*>;

        // The vector of unrolled arguments, sorted using 'sort_func'
        std::vector<single_argument> arguments;

        // The user supplied system
        UpdateFn update_func;

        // A tuple of the fully typed component pools used by this system
        TuplePools const pools;

        // A tuple of the fully typed component pools used the parent component
        parent_pool_tuple_t<parent_type> const parent_pools;

    public:
        builder_hierarchy_argument(UpdateFn update_func, SortFn /*sort*/, TuplePools const pools)
            : update_func{update_func} 
            , pools{pools}
            , parent_pools{
                std::apply([&pools](auto... parent_types) {
                    return std::make_tuple(&get_pool<decltype(parent_types)>(pools)...);
                },
                parent_types_tuple_t<parent_type>{})
            } {
        }

        TuplePools get_pools() const {
            return pools;
        }

        void run() {
            auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
            std::for_each(e_p, arguments.begin(), arguments.end(), [this](auto packed_arg) {
                if constexpr (is_entity<FirstComponent>) {
                    update_func(std::get<0>(packed_arg), extract_arg<Components>(packed_arg, 0)...);
                } else {
                    update_func(extract_arg<FirstComponent>(packed_arg, 0), extract_arg<Components>(packed_arg, 0)...);
                }
            });
        }

        // Convert a set of entities into arguments that can be passed to the system
        void build(entity_range_view entities) {
            if (entities.size() == 0) {
                arguments.clear();
                return;
            }

            // Count the total number of arguments
            size_t arg_count = 0;
            for (auto const& range : entities) {
                arg_count += range.count();
            }

            // Clear the arguments
            arguments.clear();
            //arguments.reserve(arg_count);

            // lookup for the parent
            auto& pool_parent_id = get_pool<parent_id>(pools);

            // Build the arguments for the ranges
            for (auto const& range : entities) {
                for (entity_id const& entity : range) {

                    // If the parent has sub-components specified, verify them
                    if constexpr (0 != std::tuple_size_v<decltype(parent_pools)>) {

                        // Does tests on the parent sub-components to see they satisfy the constraints
                        // ie. a 'parent<int*, float>' will return false if the parent does not have a float or
                        // has an int.
                        constexpr parent_types_tuple_t<parent_type> ptt{};
                        bool const has_parent_types = std::apply(
                            [&](auto... parent_types) {
                                auto const check_parent = [&](auto parent_type) {
                                    // Get the parent components id
                                    parent_id const pid = *pool_parent_id.find_component_data(entity);

                                    // Get the pool of the parent sub-component
                                    auto& sub_pool = get_pool<decltype(parent_type)>(pools);

                                    if constexpr (std::is_pointer_v<decltype(parent_type)>) {
                                        // The type is a filter, so the parent is _not_ allowed to have this component
                                        return !sub_pool.has_entity(pid);
                                    } else {
                                        // The parent must have this component
                                        return sub_pool.has_entity(pid);
                                    }
                                };

                                return (check_parent(parent_types) && ...);
                        }, ptt);

                        if (!has_parent_types)
                            continue;
                    }

                    if constexpr (is_entity<FirstComponent>) {
                        arguments.emplace_back(entity,
                            get_component<Components>(entity, pools)...);
                    } else {
                        arguments.emplace_back(entity,
                            get_component<FirstComponent>(entity, pools),
                            get_component<Components>(entity, pools)...);
                    }
                }
            }

            // Re-arrange the arguments to match a tree
            rebuild_tree();
        }

    private:
        void depth_first_search(entity_type ent, std::vector<single_argument>& vec, relation_mmap& parent_argument) {
            auto const [first, last] = parent_argument.equal_range(ent);
            auto current = first;

            while (current != last) {
                if (current->second != nullptr) {
                    single_argument const& argument = *current->second;
                    current->second = nullptr; // mark the node as visited

                    vec.push_back(argument);

                    auto const node = std::get<0>(argument);
                    depth_first_search(node, vec, parent_argument);
                }

                ++current;
            }
        }

        void rebuild_tree() {
            relation_map entity_argument;
            relation_mmap parent_argument;
            entity_argument.reserve(arguments.size());

            // Map entities and their parents to their arguments
            std::for_each(arguments.begin(), arguments.end(), [&](auto const& packed_arg) {
                entity_type const id = std::get<0>(packed_arg);
                entity_type const par = std::get<parent_type>(packed_arg);

                entity_argument.emplace(id, &packed_arg);
                parent_argument.emplace(par, &packed_arg);
            });

            // Find roots
            std::unordered_set<entity_type> roots;
            for (auto const& packed_arg : arguments) {
                entity_type const entity_parent = std::get<parent_type>(packed_arg);

                // If the parent points to an entity which itself does not have a parent,
                // then it is definitely a root
                if (!entity_argument.contains(entity_parent)) {
                    roots.insert(entity_parent);
                } else {
                    // cyclical trees
                }
            }

            // Do the depth-first search
            std::vector<single_argument> rearranged_args;
            rearranged_args.reserve(arguments.size());
            if (roots.size() == 0) {
                // If no roots were found, it's most likely a cyclical graph,
                // so just use the first argument as the root
                auto const root = std::get<0>(arguments[0]);
                depth_first_search(root, rearranged_args, parent_argument);
            } else {
                // Traverse the roots and re-arrange the arguments
                for (entity_type const& root : roots) {
                    depth_first_search(root, rearranged_args, parent_argument);
                }
            }

            if (rearranged_args.size() != arguments.size()) {
                // Remove the processed arguments from the map
                auto const is_arg_processed = [](auto const& arg) { return arg.second == nullptr; };
                std::erase_if(parent_argument, is_arg_processed);

                while (parent_argument.size() > 0) {
                    auto const root = parent_argument.begin()->first;
                    depth_first_search(root, rearranged_args, parent_argument);

                    std::erase_if(parent_argument, is_arg_processed);
                }
            }

            Expects(rearranged_args.size() == arguments.size());
            arguments = std::move(rearranged_args);
        }
    };
} // namespace ecs::detail

#endif // !__BUILDER_HIERARCHY_ARGUMENT_H_

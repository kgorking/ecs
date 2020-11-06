#ifndef __BUILDER_HIERARCHY_ARGUMENT_H_
#define __BUILDER_HIERARCHY_ARGUMENT_H_

// !Only to be included by system.h
#include <unordered_map>
#include <unordered_set>

namespace ecs::detail {
    using relation = std::pair<entity_id, parent>; // node, parent

    template<typename Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    struct builder_hierarchy_argument {
        // Determine the execution policy from the options (or lack thereof)
        using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(),
            std::execution::sequenced_policy, std::execution::parallel_policy>;

        builder_hierarchy_argument(
            UpdateFn update_func, SortFn /*sort*/, pool<FirstComponent> first_pool, pool<Components>... pools)
            : pools{first_pool, pools...}
            , update_func{update_func} {
        }

        builder_hierarchy_argument(UpdateFn update_func, SortFn /*sort*/, pool<Components>... pools)
            : pools{pools...}
            , update_func{update_func} {
        }

        tup_pools<FirstComponent, Components...> get_pools() const {
            return pools;
        }

        void run() {
            // Sort the arguments if the component data has been modified
            if (needs_sorting) {
                depth_first_sort();
                needs_sorting = false;
            }

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

            // Reserve space for the arguments
            arguments.clear();
            arguments.reserve(arg_count);

            // Build the arguments for the ranges
            for (auto const& range : entities) {
                for (entity_id const& entity : range) {
                    if constexpr (is_entity<FirstComponent>) {
                        arguments.emplace_back(entity, get_component<Components>(entity, pools)...);
                    } else {
                        arguments.emplace_back(entity, get_component<FirstComponent>(entity, pools),
                            get_component<Components>(entity, pools)...);
                    }
                }
            }

            needs_sorting = true;
        }

    private:
        void walk_node(entity_type node, std::vector<entity_type>& vec,
            std::unordered_multimap<entity_type, entity_type> const& node_children) {

            if (node_children.contains(node)) {
                auto [first, last] = node_children.equal_range(node);
                while (first != last) {
                    vec.push_back(first->second);
                    walk_node(first->second, vec, node_children);
                    ++first;
                }
            }
        }

        void depth_first_sort() {
            std::unordered_multimap<entity_type, entity_type> node_parent;
            std::unordered_multimap<entity_type, entity_type> node_children;
            node_parent.reserve(arguments.size());
            node_children.reserve(arguments.size());

            std::for_each(arguments.begin(), arguments.end(), [&, this](auto const& packed_arg) {
                auto const id = std::get<0>(packed_arg);
                auto const par = *std::get<parent*>(packed_arg);

                node_parent.emplace(id, par);
                node_children.emplace(par, id);
            });

            // find roots
            std::unordered_set<entity_type> roots;
            for (auto const [node, parent] : node_parent) {
                if (!node_parent.contains(parent))
                    roots.insert(parent);
            }

            // walk the tree
            std::vector<entity_type> vec;
            vec.reserve(arguments.size());
            for (auto root : roots) {
                walk_node(root, vec, node_children);
            }

            // rearrange the arguments
            std::unordered_map<entity_type, std::ptrdiff_t> node_offset;
            int offset = 0;
            for (auto id : vec) {
                node_offset[id] = offset++;
            }

            std::sort(arguments.begin(), arguments.end(), [&node_offset, &vec](auto const& l, auto const& r) {
                return node_offset[std::get<0>(l)] < node_offset[std::get<0>(r)];
            });
        }

    private:
        // Holds a single entity id and its arguments
        using single_argument =
            decltype(std::tuple_cat(std::tuple<entity_id>{0}, argument_tuple<FirstComponent, Components...>{}));

        // A tuple of the fully typed component pools used by this system
        tup_pools<FirstComponent, Components...> const pools;


        // The user supplied system
        UpdateFn update_func;

        // The vector of unrolled arguments, sorted using 'sort_func'
        std::vector<single_argument> arguments;

        // True if the data needs to be sorted
        bool needs_sorting = false;
    };
} // namespace ecs::detail

#endif // !__BUILDER_HIERARCHY_ARGUMENT_H_

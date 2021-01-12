#ifndef __SYSTEM_HIERARCHY_H_
#define __SYSTEM_HIERARCHY_H_

#include <map>
#include <unordered_map>
#include <unordered_set>

#include "system.h"
#include "find_entity_pool_intersections.h"
#include "../parent.h"

namespace ecs::detail {
    template<class Options, class UpdateFn, class TupPools, class FirstComponent, class... Components>
    class system_hierarchy final : public system<Options, UpdateFn, TupPools, FirstComponent, Components...> {
        // Holds a single entity id and its arguments
        using single_argument = decltype(
            std::tuple_cat(std::tuple<entity_id>{0}, std::declval<argument_tuple<FirstComponent, Components...>>()));

        using relation_mmap = std::multimap<entity_type, entity_type>;

    public:
        system_hierarchy(UpdateFn update_func, TupPools pools)
            : system<Options, UpdateFn, TupPools, FirstComponent, Components...>{update_func, pools}
            , parent_pools{
                std::apply([&pools](auto... parent_types) {
                    return std::make_tuple(&get_pool<decltype(parent_types)>(pools)...);
                },
                parent_types_tuple_t<parent_type>{})
            } {
        }

    private:
        void do_run() override {
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
        void do_build(entity_range_view ranges) override {
            if (ranges.size() == 0) {
                arguments.clear();
                return;
            }

            // Count the total number of arguments
            size_t arg_count = 0;
            for (auto const& range : ranges) {
                arg_count += range.count();
            }

            // Clear the arguments and reserve space for new ones
            arguments.clear();
            arguments.reserve(arg_count);

            // The component pool for the parent ids
            component_pool<parent_id> const& pool_parent_id = get_pool<parent_id>(this->pools);

            // • find a vertex with no incoming edges, put it in the output;
            // • delete all its outgoing edges;
            // • repeat.

            // Find the roots; map parents to their children.
            // 'ranges' holds all the entities that has a 'ecs::parent' on them, so any parent id I test
            // against the pool will tell me if it is a root if it doesn't exist in the pool.
            std::unordered_set<entity_type> roots;
            relation_mmap parent_argument;

            for (auto const& range : ranges) {
                // Get the ranges span of the parent-id components
                auto const span_parent_ids =
                    std::span{pool_parent_id.find_component_data(range.first()), range.count()};

                for (auto const id : range) {
                    // Get the parent id
                    auto const offset = id - range.first();
                    entity_type const parent_id = span_parent_ids[offset];

                    // Entities can't be parents of themselves
                    Expects(id != parent_id);

                    if (!pool_parent_id.has_entity(parent_id))
                        roots.insert(parent_id);

                    // Map the parent to its children
                    parent_argument.emplace(parent_id, id);
                }
            }

            // Do the depth-first search on the roots
            std::vector<entity_type> rearranged_args;
            rearranged_args.reserve(arg_count);
            for (entity_type const& root : roots) {
                depth_first_search(root, rearranged_args, parent_argument);
            }

            // This contract triggers on cyclical graphs, which are not supported
            Expects(rearranged_args.size() == arg_count);

            // Build the arguments for the ranges
            for (entity_id const entity : rearranged_args) {
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
                                auto const& sub_pool = get_pool<decltype(parent_type)>(this->pools);

                                if constexpr (std::is_pointer_v<decltype(parent_type)>) {
                                    // The type is a filter, so the parent is _not_ allowed to have this component
                                    return !sub_pool.has_entity(pid);
                                } else {
                                    // The parent must have this component
                                    return sub_pool.has_entity(pid);
                                }
                            };

                            return (check_parent(parent_types) && ...);
                        },
                        ptt);

                    if (!has_parent_types)
                        continue;
                }

                if constexpr (is_entity<FirstComponent>) {
                    arguments.emplace_back(entity, get_component<Components>(entity, this->pools)...);
                } else {
                    arguments.emplace_back(entity, get_component<FirstComponent>(entity, this->pools),
                        get_component<Components>(entity, this->pools)...);
                }
            }
        }

        void depth_first_search(entity_type ent, std::vector<entity_type>& vec, relation_mmap& parent_argument) {
            // Get all the entities that has 'ent' as a parent
            auto const [first, last] = parent_argument.equal_range(ent);
            auto current = first;

            while (current != last) {
                if (current->second != ent) {
                    entity_type const id = current->second;
                    current->second = ent; // mark the node as visited (points to itself)

                    vec.push_back(id);

                    depth_first_search(id, vec, parent_argument);
                }

                ++current;
            }
        }

    private:
        // Walking the tree in parallel doesn't seem possible
        using execution_policy = std::execution::sequenced_policy;

        // Extract the parent type
        static constexpr int ParentIndex = test_option_index<is_parent,
            std::tuple<std::remove_cvref_t<FirstComponent>, std::remove_cvref_t<Components>...>>;
        static_assert(-1 != ParentIndex, "no parent component found");

        using full_parent_type = std::tuple_element_t<ParentIndex, std::tuple<FirstComponent, Components...>>;
        using parent_type = std::remove_cvref_t<full_parent_type>;

        // The vector of unrolled arguments
        std::vector<single_argument> arguments;

        // A tuple of the fully typed component pools used the parent component
        parent_pool_tuple_t<parent_type> const parent_pools;
    };
} // namespace ecs::detail

#endif // !__SYSTEM_HIERARCHY_H_

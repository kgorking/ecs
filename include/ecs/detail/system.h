#ifndef __SYSTEM
#define __SYSTEM

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "../entity_id.h"
#include "../entity_range.h"
#include "../system_base.h"
#include "component_pool.h"
#include "type_hash.h"

namespace ecs::detail {
    template<typename T>
    constexpr bool is_read_only() {
        return detail::immutable<T> || detail::tagged<T> || std::is_const_v<std::remove_reference_t<T>>;
    }

    template<bool ignore_first_arg, typename First, typename... Types>
    constexpr auto get_type_read_only() {
        if constexpr (!ignore_first_arg) {
            std::array<bool, 1 + sizeof...(Types)> arr{is_read_only<First>(), is_read_only<Types>()...};
            return arr;
        } else {
            std::array<bool, sizeof...(Types)> arr{is_read_only<Types>()...};
            return arr;
        }
    }

    // Gets the type a sorting functions operates on
    template<class R, class C, class T1, class T2>
    struct get_sort_func_type_impl {
        get_sort_func_type_impl(R (C::*)(T1, T2) const) {
        }
        using type = std::remove_cvref_t<T1>;
    };
    template<class F>
    using sort_func_type = typename decltype(get_sort_func_type_impl(&F::operator()))::type;

    // Returns true if the type is an entity_id
    template<class T>
    constexpr bool is_entity() {
        return std::is_same_v<T, entity_id>;
    }

    // Alias for stored pools
    template<class T>
    using pool = component_pool<std::remove_pointer_t<std::remove_cvref_t<T>>>* const;

    // Get a component pool from a component pool tuple
    template<typename Component, typename Pools>
    component_pool<Component>& get_pool(Pools const& pools) {
        return *std::get<pool<Component>>(pools);
    }

    // Get an entities component from a component pool
    template<typename Component, typename Pools>
    [[nodiscard]] Component* get_component(entity_id const entity, Pools const& pools) {
        if constexpr (std::is_pointer_v<Component>) {
            static_cast<void>(entity);
            return nullptr;
        } else {
            return get_pool<Component>(pools).find_component_data(entity);
        }
    }

    // Holds a pointer to the first component from each pool
    template<class FirstComponent, class... Components>
    using argument_tuple = std::conditional_t<is_entity<FirstComponent>(),
        std::tuple<std::remove_cvref_t<Components>*...>,
        std::tuple<std::remove_cvref_t<FirstComponent>*, std::remove_cvref_t<Components>*...>>;

    // Tuple holding component pools
    template<class FirstComponent, class... Components>
    using tup_pools = std::conditional_t<is_entity<FirstComponent>(),
        std::tuple<pool<Components>...>,
        std::tuple<pool<FirstComponent>, pool<Components>...>>;


    // Manages arguments using ranges. Very fast linear traversal and minimal storage overhead.
    template<class ExePolicy, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    struct ranged_argument_builder {
        ranged_argument_builder(UpdateFn update_func, SortFn /*sort*/, pool<FirstComponent> first_pool, pool<Components>... pools)
            : pools{first_pool, pools...}
            , update_func{update_func} {
        }

        ranged_argument_builder(UpdateFn update_func, SortFn /*sort*/, pool<Components>... pools)
            : pools{pools...}
            , update_func{update_func} {
        }

        tup_pools<FirstComponent, Components...> get_pools() const {
            return pools;
        }

        void run() {
            // Small helper function
            auto const extract_arg = [](auto ptr, [[maybe_unused]] ptrdiff_t offset) -> decltype(auto) {
                using T = std::remove_cvref_t<decltype(*ptr)>;
                if constexpr (std::is_pointer_v<T>) {
                    return nullptr;
                } else if constexpr (detail::unbound<T>) {
                    return *ptr;
                } else {
                    return *(ptr + offset);
                }
            };

            // Call the system for all pairs of components that match the system signature
            for (auto const& argument : arguments) {
                auto const& range = std::get<entity_range>(argument);
                std::for_each(ExePolicy{}, range.begin(), range.end(),
                    [extract_arg, this, &argument, first_id = range.first()](auto ent) {
                        auto const offset = ent - first_id;
                        if constexpr (is_entity<FirstComponent>()) {
                            update_func(
                                ent, extract_arg(std::get<std::remove_cvref_t<Components>*>(argument), offset)...);
                        } else {
                            update_func(extract_arg(std::get<std::remove_cvref_t<FirstComponent>*>(argument), offset),
                                extract_arg(std::get<std::remove_cvref_t<Components>*>(argument), offset)...);
                        }
                    });
            }
        }

        // Convert a set of entities into arguments that can be passed to the system
        void build(entity_range_view entities) {
            // Build the arguments for the ranges
            arguments.clear();
            for (auto const& range : entities) {
                if constexpr (is_entity<FirstComponent>()) {
                    arguments.emplace_back(range, get_component<std::remove_cvref_t<Components>>(range.first(), pools)...);
                } else {
                    arguments.emplace_back(range,
                        get_component<std::remove_cvref_t<FirstComponent>>(range.first(), pools),
                        get_component<std::remove_cvref_t<Components>>(range.first(), pools)...);
                }
            }
        }

    private:
        // Holds an entity range and its arguments
        using range_argument = decltype(std::tuple_cat(std::tuple<entity_range>{{0, 1}}, argument_tuple<FirstComponent, Components...>{}));

        // A tuple of the fully typed component pools used by this system
        tup_pools<FirstComponent, Components...> const pools;

        // The user supplied system
        UpdateFn update_func;

        // Holds the arguments for a range of entities
        std::vector<range_argument> arguments;
    };

    // Manages sorted arguments. Neither cache- nor storage space friendly, but arguments
    // will be passed to the user supplied lambda in a sorted manner
    template<class ExePolicy, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    struct sorted_argument_builder {
        sorted_argument_builder(UpdateFn update_func, SortFn sort, pool<FirstComponent> first_pool, pool<Components>... pools)
            : pools{first_pool, pools...}
            , update_func{update_func}
            , sort_func{sort} {
        }

        sorted_argument_builder(UpdateFn update_func, SortFn sort, pool<Components>... pools)
            : pools{pools...}
            , update_func{update_func}
            , sort_func{sort} {
        }

        tup_pools<FirstComponent, Components...> get_pools() const {
            return pools;
        }

        void run() {
            // Sort the arguments if the component data has been modified
            if (needs_sorting || std::get<pool<sort_type>>(pools)->has_components_been_modified()) {
                std::sort(ExePolicy{}, arguments.begin(), arguments.end(), [this](auto const& l, auto const& r) {
                    sort_type* t_l = std::get<sort_type*>(l);
                    sort_type* t_r = std::get<sort_type*>(r);
                    return sort_func(*t_l, *t_r);
                });

                needs_sorting = false;
            }

            std::for_each(ExePolicy{}, arguments.begin(), arguments.end(), [this](auto packed_arg) {
                if constexpr (is_entity<FirstComponent>()) {
                    update_func(std::get<0>(packed_arg), *std::get<std::remove_cvref_t<Components>*>(packed_arg)...);
                } else {
                    update_func(*std::get<std::remove_cvref_t<FirstComponent>*>(packed_arg),
                        *std::get<std::remove_cvref_t<Components>*>(packed_arg)...);
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
                    if constexpr (is_entity<FirstComponent>()) {
                        arguments.emplace_back(
                            entity, get_component<std::remove_cvref_t<Components>>(entity, pools)...);
                    } else {
                        arguments.emplace_back(entity,
                            get_component<std::remove_cvref_t<FirstComponent>>(entity, pools),
                            get_component<std::remove_cvref_t<Components>>(entity, pools)...);
                    }
                }
            }

            needs_sorting = true;
        }

    private:
        // Holds a single entity id and its arguments
        using single_argument = decltype(std::tuple_cat(std::tuple<entity_id>{0}, argument_tuple<FirstComponent, Components...>{}));

        using sort_type = sort_func_type<SortFn>;
        static_assert(std::predicate<SortFn, sort_type, sort_type>, "Sorting function is not a predicate");

        // A tuple of the fully typed component pools used by this system
        tup_pools<FirstComponent, Components...> const pools;

        // The user supplied system
        UpdateFn update_func;

        // The user supplied sorting function
        SortFn sort_func;

        // The vector of unrolled arguments, sorted using 'sort_func'
        std::vector<single_argument> arguments;

        // True if the data needs to be sorted
        bool needs_sorting = false;
    };

    // Select the argument manager based on wheter a sorting function is supplied or not
    template<class ExePolicy, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    using builder_selector = std::conditional_t<std::is_same_v<SortFn, std::nullptr_t>,
        ranged_argument_builder<ExePolicy, UpdateFn, SortFn, FirstComponent, Components...>,
        sorted_argument_builder<ExePolicy, UpdateFn, SortFn, FirstComponent, Components...>>;

    // The implementation of a system specialized on its components
    template<int Group, class ExePolicy, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    class system final : public system_base {
        using argument_builder = builder_selector<ExePolicy, UpdateFn, SortFn, FirstComponent, Components...>;
        argument_builder arguments;

    public:
        // Constructor for when the first argument to the system is _not_ an entity
        system(UpdateFn update_func, SortFn sort_func, pool<FirstComponent> first_pool, pool<Components>... pools)
            : arguments{update_func, sort_func, first_pool, pools...} {
            find_entities();
        }

        // Constructor for when the first argument to the system _is_ an entity
        system(UpdateFn update_func, SortFn sort_func, pool<Components>... pools)
            : arguments{update_func, sort_func, pools...} {
            find_entities();
        }

        void run() override {
            if (!is_enabled()) {
                return;
            }

            arguments.run();

            // Notify pools if data was written to them
            if constexpr (!is_entity<FirstComponent>()) {
                notify_pool_modifed<FirstComponent>();
            }
            (notify_pool_modifed<Components>(), ...);
        }

        template<typename T>
        void notify_pool_modifed() {
            if constexpr (!is_read_only<T>() && !std::is_pointer_v<T>) {
                get_pool<std::remove_cvref_t<T>>().notify_components_modified();
            }
        }

        constexpr int get_group() const noexcept override {
            return Group;
        }

        std::string get_signature() const noexcept override {
            // Component names
            constexpr std::array<std::string_view, num_arguments> argument_names{
                get_type_name<FirstComponent>(), get_type_name<Components>()...};

            std::string sig("system(");
            for (size_t i = 0; i < num_arguments - 1; i++) {
                sig += argument_names[i];
                sig += ", ";
            }
            sig += argument_names[num_arguments - 1];
            sig += ')';
            return sig;
        }

        constexpr std::span<detail::type_hash const> get_type_hashes() const noexcept override {
            return type_hashes;
        }

        constexpr bool has_component(detail::type_hash hash) const noexcept override {
            return type_hashes.end() != std::find(type_hashes.begin(), type_hashes.end(), hash);
        }

        constexpr bool depends_on(system_base const* other) const noexcept override {
            for (auto hash : get_type_hashes()) {
                // If the other system doesn't touch the same component,
                // then there can be no dependecy
                if (!other->has_component(hash))
                    continue;

                bool const other_writes = other->writes_to_component(hash);
                if (other_writes) {
                    // The other system writes to the component,
                    // so there is a strong dependency here.
                    // Order is preserved.
                    return true;
                } else { // 'other' reads component
                    bool const this_writes = writes_to_component(hash);
                    if (this_writes) {
                        // This system writes to the component,
                        // so there is a strong dependency here.
                        // Order is preserved.
                        return true;
                    } else {
                        // These systems have a weak read/read dependency
                        // and can be scheduled concurrently
                        // Order does not need to be preserved.
                        continue;
                    }
                }
            }

            return false;
        }

        constexpr bool writes_to_any_components() const noexcept override {
            if constexpr (!is_entity<FirstComponent>() && !std::is_const_v<std::remove_reference_t<FirstComponent>>)
                return true;
            else {
                return ((!std::is_const_v<std::remove_reference_t<Components>>) &&...);
            }
        }

        constexpr bool writes_to_component(detail::type_hash hash) const noexcept override {
            auto const it = std::find(type_hashes.begin(), type_hashes.end(), hash);
            if (it == type_hashes.end())
                return false;

            // Contains true if a type is read-only
            constexpr std::array<bool, num_components> type_read_only =
                get_type_read_only<is_entity<FirstComponent>(), FirstComponent, Components...>();

            return !type_read_only[std::distance(type_hashes.begin(), it)];
        }

    private:
        // Handle changes when the component pools change
        void process_changes(bool force_rebuild) override {
            if (force_rebuild) {
                find_entities();
                return;
            }

            if (!is_enabled()) {
                return;
            }

            bool const modified = std::apply(
                [](auto... pools) { return (pools->has_component_count_changed() || ...); }, arguments.get_pools());

            if (modified) {
                find_entities();
            }
        }

        // Locate all the entities affected by this system
        // and send them to the argument builder
        void find_entities() {
            if constexpr (num_components == 1) {
                // Build the arguments
                entity_range_view const entities = std::get<0>(arguments.get_pools())->get_entities();
                arguments.build(entities);
            } else {
                // When there are more than one component required for a system,
                // find the intersection of the sets of entities that have those components

                // The intersector
                std::vector<entity_range> ranges;
                auto const intersect = [&](auto arg) {
                    using type = std::remove_pointer_t<decltype(arg)>;
                    if constexpr (std::is_pointer_v<type>) {
                        // Skip pointers
                        return;
                    } else {
                        auto const& type_pool = get_pool<type>();

                        if (ranges.empty()) {
                            entity_range_view const span = type_pool.get_entities();
                            ranges.insert(ranges.end(), span.begin(), span.end());
                        } else {
                            ranges = intersect_ranges(ranges, type_pool.get_entities());
                        }
                    }
                };

                // Find the intersections
                auto dummy = argument_tuple<FirstComponent, Components...>{};
                std::apply([&intersect](auto... args) { (..., intersect(args)); }, dummy);

                // Filter out types if needed
                if constexpr (num_filters > 0) {
                    auto const difference = [&](auto arg) {
                        using type = std::remove_pointer_t<decltype(arg)>;
                        if constexpr (std::is_pointer_v<type>) {
                            auto const& type_pool = get_pool<std::remove_pointer_t<type>>();
                            ranges = difference_ranges(ranges, type_pool.get_entities());
                        }
                    };

                    if (!ranges.empty())
                        std::apply([&difference](auto... args) { (..., difference(args)); }, dummy);
                }

                // Build the arguments
                arguments.build(ranges);
            }
        }

        template<typename Component>
        [[nodiscard]] component_pool<Component>& get_pool() const {
            return detail::get_pool<Component>(arguments.get_pools());
        }

    private:
        // Number of arguments
        static constexpr size_t num_arguments = 1 + sizeof...(Components);

        // Number of components
        static constexpr size_t num_components = sizeof...(Components) + !is_entity<FirstComponent>();

        // Number of filters
        static constexpr size_t num_filters = (std::is_pointer_v<FirstComponent> + ... + std::is_pointer_v<Components>);
        static_assert(num_filters < num_components, "systems must have at least one non-filter component");

        // Hashes of stripped types used by this system ('int' instead of 'int const&')
        static constexpr std::array<detail::type_hash, num_components> type_hashes =
            get_type_hashes_array<is_entity<FirstComponent>(), std::remove_cvref_t<FirstComponent>,
                std::remove_cvref_t<Components>...>();
    };
} // namespace ecs::detail

#endif // !__SYSTEM

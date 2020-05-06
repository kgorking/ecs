#ifndef __SYSTEM
#define __SYSTEM

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "component_pool.h"
#include "entity_id.h"
#include "entity_range.h"
#include "system_base.h"
#include "type_hash.h"

namespace ecs::detail {
    template<bool ignore_first_arg, typename First, typename... Types>
    constexpr auto get_type_hashes_array() {
        if constexpr (!ignore_first_arg) {
            std::array<detail::type_hash, 1 + sizeof...(Types)> arr{get_type_hash<First>(), get_type_hash<Types>()...};
            return arr;
        } else {
            std::array<detail::type_hash, sizeof...(Types)> arr{get_type_hash<Types>()...};
            return arr;
        }
    }

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

    // clang-format off
    // Gets the type a sorting functions operates on
    template<class R, class C, class T1, class T2>
    struct get_sort_func_type_impl {
        get_sort_func_type_impl(R (C::*)(T1, T2) const) { }
        using type = std::remove_cvref_t<T1>;
    };
    template <class T>
    using sort_func_type = typename decltype(get_sort_func_type_impl(&T::operator ()))::type;
    // clang-format on

    // The implementation of a system specialized on its components
    template<int Group, class ExecutionPolicy, typename UpdateFunc, typename SortFunc, class FirstComponent,
        class... Components>
    class system final : public system_base {

        template<typename T>
        using rcv = std::remove_cvref_t<T>;

        // Determines if the first component is an entity
        static constexpr bool is_first_arg_entity =
            std::is_same_v<FirstComponent, entity_id> || std::is_same_v<FirstComponent, entity>;

        // Number of arguments
        static constexpr size_t num_arguments = 1 + sizeof...(Components);

        // Calculate the number of components
        static constexpr size_t num_components = sizeof...(Components) + (is_first_arg_entity ? 0 : 1);

        // Calculate the number of filters
        static constexpr size_t num_filters = (std::is_pointer_v<FirstComponent> + ... + std::is_pointer_v<Components>);
        static_assert(num_filters < num_components, "systems can not consist of just filters");

        // Component names
        static constexpr std::array<std::string_view, num_arguments> argument_names =
            std::to_array({get_type_name<FirstComponent>(), get_type_name<Components>()...});

        // Hashes of stripped types used by this system ('int' instead of 'int const&')
        static constexpr std::array<detail::type_hash, num_components> type_hashes =
            get_type_hashes_array<is_first_arg_entity, rcv<FirstComponent>, rcv<Components>...>();

        // Contains true if a type is read-only
        static constexpr std::array<bool, num_components> type_read_only =
            get_type_read_only<is_first_arg_entity, FirstComponent, Components...>();

        // Alias for stored pools
        template<class T>
        using pool = component_pool<std::remove_pointer_t<rcv<T>>>* const;

        // Tuple holding all pools used by this system
        using tup_pools = std::conditional_t<is_first_arg_entity, std::tuple<pool<Components>...>,
            std::tuple<pool<FirstComponent>, pool<Components>...>>;

        // Holds a pointer to the first component from each pool
        using argument_tuple = std::conditional_t<is_first_arg_entity, std::tuple<rcv<Components>*...>,
            std::tuple<rcv<FirstComponent>*, rcv<Components>*...>>;

        // Holds an entity range and its arguments
        using range_argument = decltype(std::tuple_cat(std::tuple<entity_range>{{0, 1}}, argument_tuple{}));

        // Holds a single entity id and its arguments
        using packed_argument = decltype(std::tuple_cat(std::tuple<entity_id>{0}, argument_tuple{}));

        // Holds the arguments for a range of entities
        std::vector<range_argument> arguments;

        // A tuple of the fully typed component pools used by this system
        tup_pools const pools;

        // The user supplied system
        UpdateFunc update_func;

        // True if a valid sorting function is supplied
        static constexpr bool has_sort_func = !std::is_same_v<std::nullptr_t, SortFunc>;

        // The user supplied sorting function
        SortFunc sort_func;

        // The vector of unrolled arguments, sorted using 'sort_func'
        std::vector<packed_argument> sorted_arguments;

    public:
        // Constructor for when the first argument to the system is _not_ an entity
        system(UpdateFunc update_func, SortFunc sort_func, pool<FirstComponent> first_pool, pool<Components>... pools)
            : pools{first_pool, pools...}, update_func{update_func}, sort_func{sort_func} {
            build_args();
        }

        // Constructor for when the first argument to the system _is_ an entity
        system(UpdateFunc update_func, SortFunc sort_func, pool<Components>... pools)
            : pools{pools...}, update_func{update_func}, sort_func{sort_func} {
            build_args();
        }

        void update() override {
            if (!is_enabled()) {
                return;
            }

            if constexpr (has_sort_func) {
                std::for_each(
                    ExecutionPolicy{}, sorted_arguments.begin(), sorted_arguments.end(), [this](auto packed_arg) {
                        if constexpr (is_first_arg_entity) {
                            update_func(std::get<0>(packed_arg), *std::get<rcv<Components>*>(packed_arg)...);
                        } else {
                            update_func(*std::get<rcv<FirstComponent>*>(packed_arg),
                                *std::get<rcv<Components>*>(packed_arg)...);
                        }
                    });
            } else {
                // Small helper function
                auto const extract_arg = [](auto ptr, [[maybe_unused]] ptrdiff_t offset) -> decltype(auto){
                    using T = std::remove_cvref_t<decltype(*ptr)>;
                    if constexpr (std::is_pointer_v<T>) {
                        return nullptr;
                    }
                    else if constexpr (detail::unbound<T>) {
                        return *ptr;
                    } else {
                        return *(ptr + offset);
                    }
                };

                // Call the system for all pairs of components that match the system signature
                for (auto const& argument : arguments) {
                    auto const& range = std::get<entity_range>(argument);
                    std::for_each(ExecutionPolicy{}, range.begin(), range.end(),
                        [extract_arg, this, &argument, first_id = range.first()](auto ent) {
                            auto const offset = ent - first_id;
                            if constexpr (is_first_arg_entity) {
                                update_func(ent, extract_arg(std::get<rcv<Components>*>(argument), offset)...);
                            } else {
                                update_func(extract_arg(std::get<rcv<FirstComponent>*>(argument), offset),
                                    extract_arg(std::get<rcv<Components>*>(argument), offset)...);
                            }
                        });
                }
            }
        }

        constexpr int get_group() const noexcept override { return Group; }

        std::string get_signature() const noexcept override {
            std::string sig("system(");
            for (size_t i = 0; i < num_arguments - 1; i++) {
                sig += argument_names[i];
                sig += ", ";
            }
            sig += argument_names[num_arguments - 1];
            sig += ')';
            return sig;
        }

        constexpr std::span<detail::type_hash const> get_type_hashes() const noexcept override { return type_hashes; }

        constexpr bool has_component(detail::type_hash hash) const noexcept override {
            return static_has_component(hash);
        }

        static constexpr bool static_has_component(detail::type_hash hash) noexcept {
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
            if constexpr (!is_first_arg_entity && !std::is_const_v<std::remove_reference_t<FirstComponent>>)
                return true;
            else {
                return ((!std::is_const_v<std::remove_reference_t<Components>>) &&...);
            }
        }

        constexpr bool writes_to_component(detail::type_hash hash) const noexcept override {
            auto const it = std::find(type_hashes.begin(), type_hashes.end(), hash);
            if (it == type_hashes.end())
                return false;

            return !type_read_only[std::distance(type_hashes.begin(), it)];
        }

    protected:
    private:
        // Handle changes when the component pools change
        void process_changes(bool force_rebuild) override {
            if (force_rebuild) {
                build_args();
                return;
            }

            if (!is_enabled()) {
                return;
            }

            auto constexpr is_pools_modified = [](auto... pools) { return (pools->is_data_modified() || ...); };
            bool const is_modified = std::apply(is_pools_modified, pools);

            if (is_modified) {
                build_args();
            }
        }

        void build_args() {
            if constexpr (has_sort_func) {
                // Check that the system has the type that sort_func wants to sort on
                static_assert(static_has_component(get_type_hash<sort_func_type<SortFunc>>()),
                    "sorting function operates on a type that the system does not have");
            }

            if constexpr (num_components == 1) {
                if constexpr (!std::is_pointer_v<std::remove_pointer_t<std::tuple_element_t<0, argument_tuple>>>) {
                    // Build the arguments
                    entity_range_view const entities = std::get<0>(pools)->get_entities();
                    build_args(entities);
                }
            } else {
                // When there are more than one component required for a system,
                // find the intersection of the sets of entities that have those components

                // Intersects two ranges of entities
                auto const intersect_ranges = [](entity_range_view view_a, entity_range_view view_b) {
                    std::vector<entity_range> result;

                    if (view_a.empty() || view_b.empty()) {
                        return result;
                    }

                    auto it_a = view_a.cbegin();
                    auto it_b = view_b.cbegin();

                    while (it_a != view_a.cend() && it_b != view_b.cend()) {
                        if (it_a->overlaps(*it_b)) {
                            result.push_back(entity_range::intersect(*it_a, *it_b));
                        }

                        if (it_a->last() < it_b->last()) { // range a is inside range b, move to
                                                           // the next range in a
                            ++it_a;
                        } else if (it_b->last() < it_a->last()) { // range b is inside range a,
                                                                  // move to the next range in b
                            ++it_b;
                        } else { // ranges are equal, move to next ones
                            ++it_a;
                            ++it_b;
                        }
                    }

                    return result;
                };

                auto const difference_ranges = [](entity_range_view view_a, entity_range_view view_b) {
                    std::vector<entity_range> result;

                    if (view_a.empty() || view_b.empty()) {
                        return result;
                    }

                    auto it_a = view_a.cbegin();
                    auto it_b = view_b.cbegin();

                    while (it_a != view_a.cend() && it_b != view_b.cend()) {
                        if (it_a->overlaps(*it_b)) {
                            auto res = entity_range::remove(*it_a, *it_b);
                            result.push_back(res.first);
                            if (res.second.has_value())
                                result.push_back(res.second.value());
                        }

                        if (it_a->last() < it_b->last()) { // range a is inside range b, move to
                                                           // the next range in a
                            ++it_a;
                        } else if (it_b->last() < it_a->last()) { // range b is inside range a,
                                                                  // move to the next range in b
                            ++it_b;
                        } else { // ranges are equal, move to next ones
                            ++it_a;
                            ++it_b;
                        }
                    }

                    return result;
                };

                // Intersect the entity ranges
                // auto const intersect = do_intersection(entities, get_pool<rcv<Components>>().get_entities()...);

                // The intersector
                std::optional<std::vector<entity_range>> ranges;
                auto const intersect = [&, this](auto arg) { // arg = rcv<Components>*
                    using type = std::remove_pointer_t<decltype(arg)>;
                    if constexpr (std::is_pointer_v<type>) {
                        // Skip pointers
                        return;
                    } else {
                        auto const& type_pool = get_pool<type>();
                        // auto& type_pool = *std::get<pool<type>>(pools);

                        if (ranges.has_value()) {
                            ranges = intersect_ranges(*ranges, type_pool.get_entities());
                        } else {
                            auto span = type_pool.get_entities();
                            ranges.emplace(span.begin(), span.end());
                        }
                    }
                };

                auto const difference = [&, this](auto arg) { // arg = rcv<Components>*
                    using type = std::remove_pointer_t<decltype(arg)>;
                    if constexpr (std::is_pointer_v<type>) {
                        auto& type_pool = get_pool<std::remove_pointer_t<type>>();
                        ranges = difference_ranges(*ranges, type_pool.get_entities());
                    }
                };

                // Find the intersections and differences
                auto dummy = argument_tuple{};
                std::apply([&intersect](auto... args) { (..., intersect(args)); }, dummy);
                std::apply([&difference](auto... args) { (..., difference(args)); }, dummy);

                // Build the arguments
                if (ranges.has_value())
                    build_args(ranges.value());
                else
                    arguments.clear();
            }

            // Unpack the arguments and sort them
            if constexpr (has_sort_func) {
                // Count the total number of arguments
                size_t arg_count = 0;
                for (auto const& args : arguments) { arg_count += std::get<0>(args).count(); }

                // Unpack the arguments
                sorted_arguments.clear();
                sorted_arguments.reserve(arg_count);
                for (auto const& args : arguments) {
                    for (entity_id const& entity : std::get<0>(args)) {
                        if constexpr (is_first_arg_entity) {
                            sorted_arguments.emplace_back(entity, get_component<rcv<Components>>(entity)...);
                        } else {
                            sorted_arguments.emplace_back(entity, get_component<rcv<FirstComponent>>(entity),
                                get_component<rcv<Components>>(entity)...);
                        }
                    }
                }

                // Sort the arguments
                using sort_type = sort_func_type<SortFunc>;
                std::sort(sorted_arguments.begin(), sorted_arguments.end(), [this](auto const& l, auto const& r) {
                    sort_type* t_l = std::get<sort_type*>(l);
                    sort_type* t_r = std::get<sort_type*>(r);
                    return sort_func(*t_l, *t_r);
                });
            }
        }

        // Convert a set of entities into arguments that can be passed to the system
        void build_args(entity_range_view entities) {
            // Build the arguments for the ranges
            arguments.clear();
            for (auto const& range : entities) {
                if constexpr (is_first_arg_entity) {
                    arguments.emplace_back(range, get_component<rcv<Components>>(range.first())...);
                } else {
                    arguments.emplace_back(range, get_component<rcv<FirstComponent>>(range.first()),
                        get_component<rcv<Components>>(range.first())...);
                }
            }
        }

        template<typename Component>
        [[nodiscard]] component_pool<Component>& get_pool() const {
            return *std::get<pool<Component>>(pools);
        }

        template<typename Component>
        [[nodiscard]] Component* get_component(entity_id const entity) {
            if constexpr (std::is_pointer_v<Component>) {
                static_cast<void>(entity);
                static Component* dummy = nullptr;
                return dummy;
            } else {
                return get_pool<Component>().find_component_data(entity);
            }
        }
    }; // namespace ecs::detail
} // namespace ecs::detail

#endif // !__SYSTEM

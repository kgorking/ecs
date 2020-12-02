#ifndef __COMPONENT_POOL
#define __COMPONENT_POOL

#include <functional>
#include <tuple>
#include <type_traits>
#include <vector>
#include <execution>
#include <cstring> // for memcmp

#include "tls/splitter.h"

#include "../entity_id.h"
#include "../entity_range.h"
#include "parent_id.h"

#include "component_pool_base.h"
#include "flags.h"
#include "options.h"

template<class ForwardIt, class BinaryPredicate>
ForwardIt std_combine_erase(ForwardIt first, ForwardIt last, BinaryPredicate p) {
    if (first == last)
        return last;

    ForwardIt result = first;
    while (++first != last) {
        auto const pred_res = p(*result, *first);
        if (!pred_res && ++result != first) {
            *result = std::move(*first);
        }
    }
    return ++result;
}

template<class Cont, class BinaryPredicate>
void combine_erase(Cont& cont, BinaryPredicate p) {
    auto const end = std_combine_erase(cont.begin(), cont.end(), p);
    cont.erase(end, cont.end());
}

namespace ecs::detail {
    template<typename T>
    class component_pool final : public component_pool_base {
    private:
        // The components
        std::vector<T> components;

        // The entities that have components in this storage.
        std::vector<entity_range> ranges;

        // Keep track of which components to add/remove each cycle
        using entity_data = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, T>>;
        using entity_init = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, std::function<T(entity_id)>>>;
        tls::splitter<std::vector<entity_data>, component_pool<T>> deferred_adds;
        tls::splitter<std::vector<entity_init>, component_pool<T>> deferred_init_adds;
        tls::splitter<std::vector<entity_range>, component_pool<T>> deferred_removes;

        // Status flags
        bool components_added = false;
        bool components_removed = false;
        bool components_modified = false;

    public:
        // Add a component to a range of entities, initialized by the supplied user function
        // Pre: entities has not already been added, or is in queue to be added
        //      This condition will not be checked until 'process_changes' is called.
        template<typename Fn>
        void add_init(entity_range const range, Fn&& init) {
            // Add the range and function to a temp storage
            deferred_init_adds.local().emplace_back(range, std::forward<Fn>(init));
        }

        // Add a component to a range of entity.
        // Pre: entities has not already been added, or is in queue to be added
        //      This condition will not be checked until 'process_changes' is called.
        void add(entity_range const range, T&& component) {
            if constexpr (tagged<T>) {
                deferred_adds.local().push_back(range);
            } else {
                deferred_adds.local().emplace_back(range, std::forward<T>(component));
            }
        }

        // Return the shared component
        T& get_shared_component() requires unbound<T> {
            static T t{};
            return t;
        }

        // Remove an entity from the component pool. This logically removes the component from the
        // entity.
        void remove(entity_id const id) {
            remove_range({id, id});
        }

        // Remove an entity from the component pool. This logically removes the component from the
        // entity.
        void remove_range(entity_range const range) {
            deferred_removes.local().push_back(range);
        }

        // Returns an entities component.
        // Returns nullptr if the entity is not found in this pool
        T* find_component_data(entity_id const id) {
            auto const index = find_entity_index(id);
            return index ? &components[index.value()] : nullptr;
        }

        // Merge all the components queued for addition to the main storage,
        // and remove components queued for removal
        void process_changes() override {
            process_remove_components();
            process_add_components();
        }

        // Returns the number of active entities in the pool
        size_t num_entities() const {
            // Don't want to include <algorithm> just for this
            size_t val = 0;
            for (auto r : ranges) {
                val += r.count();
            }
            return val;
        }

        // Returns the number of active components in the pool
        size_t num_components() const {
            if constexpr (unbound<T>)
                return 1;
            else
                return components.size();
        }

        // Clears the pools state flags
        void clear_flags() override {
            components_added = false;
            components_removed = false;
            components_modified = false;
        }

        // Returns true if components has been added since last clear_flags() call
        bool has_more_components() const {
            return components_added;
        }

        // Returns true if components has been removed since last clear_flags() call
        bool has_less_components() const {
            return components_removed;
        }

        // Returns true if components has been added/removed since last clear_flags() call
        bool has_component_count_changed() const {
            return components_added || components_removed;
        }

        bool has_components_been_modified() const {
            return has_component_count_changed() || components_modified;
        }

        // Returns the pools entities
        entity_range_view get_entities() const {
            if constexpr (detail::global<T>) {
                // globals are accessible to all entities
                static constexpr entity_range global_range{
                    std::numeric_limits<ecs::detail::entity_type>::min(), std::numeric_limits<ecs::detail::entity_type>::max()};
                return entity_range_view{&global_range, 1};
            } else {
                return ranges;
            }
        }

        // Returns true if an entity has a component in this pool
        bool has_entity(entity_id const id) const {
            return has_entity({id, id});
        }

        // Returns true if an entity range has components in this pool
        bool has_entity(entity_range const& range) const {
            if (ranges.empty()) {
                return false;
            }

            for (entity_range const& r : ranges) {
                if (r.contains(range)) {
                    return true;
                }
            }

            return false;
        }

        // TODO remove?
        // Checks the current threads queue for the entity
        bool is_queued_add(entity_id const id) {
            return is_queued_add({id, id});
        }

        // Checks the current threads queue for the entity
        bool is_queued_add(entity_range const& range) {
            if (deferred_adds.local().empty()) {
                return false;
            }

            for (auto const& ents : deferred_adds.local()) {
                if (std::get<0>(ents).contains(range)) {
                    return true;
                }
            }

            return false;
        }

        // Checks the current threads queue for the entity
        bool is_queued_remove(entity_id const id) {
            return is_queued_remove({id, id});
        }

        // Checks the current threads queue for the entity
        bool is_queued_remove(entity_range const& range) {
            if (deferred_removes.local().empty())
                return false;

            for (auto const& ents : deferred_removes.local()) {
                if (ents.contains(range))
                    return true;
            }

            return false;
        }

        // Clear all entities from the pool
        void clear() override {
            // Remember if components was removed from the pool
            bool const is_removed = !components.empty();

            // Clear the pool
            ranges.clear();
            components.clear();
            deferred_adds.clear();
            deferred_init_adds.clear();
            deferred_removes.clear();
            clear_flags();

            // Save the removal state
            components_removed = is_removed;
        }

        // Flag that components has been modified
        void notify_components_modified() {
            components_modified = true;
        }

    private:
        // Flag that components has been added
        void set_data_added() {
            components_added = true;
        }

        // Flag that components has been removed
        void set_data_removed() {
            components_removed = true;
        }

        // Searches for an entitys offset in to the component pool.
        // Returns nothing if 'ent' is not a valid entity
        std::optional<size_t> find_entity_index(entity_id const ent) const {
            if (ranges.empty() || !has_entity(ent)) {
                return {};
            }

            // Run through the ranges
            size_t index = 0;
            for (entity_range const& range : ranges) {
                if (!range.contains(ent)) {
                    index += range.count();
                    continue;
                }

                index += range.offset(ent);
                return index;
            }

            return {};
        }

        // Add new queued entities and components to the main storage
        void process_add_components() {
            // Combine the components in to a single vector
            std::vector<entity_data> adds;
            for (auto& vec : deferred_adds) {
                std::move(vec.begin(), vec.end(), std::back_inserter(adds));
            }

            std::vector<entity_init> inits;
            for (auto& vec : deferred_init_adds) {
                std::move(vec.begin(), vec.end(), std::back_inserter(inits));
            }

            if (adds.empty() && inits.empty()) {
                return;
            }

            // Clear the current adds
            deferred_adds.clear();
            deferred_init_adds.clear();

            // Sort the input
            auto constexpr comparator = [](auto const& l, auto const& r) {
                return std::get<0>(l).first() < std::get<0>(r).first();
            };
            std::sort(std::execution::par, adds.begin(), adds.end(), comparator);
            std::sort(std::execution::par, inits.begin(), inits.end(), comparator);

            // Check the 'add*' functions precondition.
            // An entity can not have more than one of the same component
            auto const has_duplicate_entities = [](auto const& vec) {
                return vec.end() != std::adjacent_find(vec.begin(), vec.end(),
                                        [](auto const& l, auto const& r) { return std::get<0>(l) == std::get<0>(r); });
            };
            Expects(false == has_duplicate_entities(adds));

            // Merge adjacent ranges
            if constexpr (!detail::unbound<T>) { // contains data
                combine_erase(adds, [](entity_data& a, entity_data const& b) {
                    auto& [a_rng, a_data] = a;
                    auto const& [b_rng, b_data] = b;

                    if (a_rng.can_merge(b_rng) && 0 == memcmp(&a_data, &b_data, sizeof(T))) {
                        a_rng = entity_range::merge(a_rng, b_rng);
                        return true;
                    } else {
                        return false;
                    }
                });
                combine_erase(inits, [](entity_init& a, entity_init const& b) {
                    auto a_rng = std::get<0>(a);
                    auto const b_rng = std::get<0>(b);

                    #ifdef __clang__
                    auto a_func = std::get<1>(a);
                    auto b_func = std::get<1>(b);
                    #else
                    auto const a_func = std::get<1>(a);
                    auto const b_func = std::get<1>(b);
                    #endif

                    if (a_rng.can_merge(b_rng) && (a_func.template target<T(entity_id)>() ==  b_func.template target<T(entity_id)>())) {
                        a_rng = entity_range::merge(a_rng, b_rng);
                        return true;
                    } else {
                        return false;
                    }
                });
            } else { // does not contain data
                auto const combiner = [](auto& a, auto const& b) { // entity_data/entity_init
                    auto& [a_rng] = a;
                    auto const& [b_rng] = b;

                    if (a_rng.can_merge(b_rng)) {
                        a_rng = entity_range::merge(a_rng, b_rng);
                        return true;
                    } else {
                        return false;
                    }
                };
                combine_erase(adds, combiner);
                combine_erase(inits, combiner);
            }

            // Add the new entities/components
            std::vector<entity_range> new_ranges;
            auto it_adds = adds.begin();
            auto ranges_it = ranges.cbegin();

            auto const insert_range = [&](auto const it) {
                entity_range const& range = std::get<0>(*it);
                size_t offset = 0;

                // Copy the current ranges while looking for an insertion point
                while (ranges_it != ranges.cend() && (*ranges_it < range)) {
                    if constexpr (!unbound<T>) {
                        // Advance the component offset so it will point
                        // to the correct components when inserting
                        offset += ranges_it->count();
                    }

                    new_ranges.push_back(*ranges_it++);
                }

                // New range must not already exist in the pool
                if (ranges_it != ranges.cend())
                    Expects(false == ranges_it->overlaps(range));

                // Add or merge the new range
                if (!new_ranges.empty() && new_ranges.back().can_merge(range)) {
                    // Merge the new range with the last one in the vector
                    new_ranges.back() = ecs::entity_range::merge(new_ranges.back(), range);
                } else {
                    // Add the new range
                    new_ranges.push_back(range);
                }

                // return the offset
                return offset;
            };

            if constexpr (!detail::unbound<T>) {
                auto it_inits = inits.begin();
                auto component_it = components.cbegin();

                auto const insert_data = [&](size_t offset) {
                    // Add the new data
                    component_it += offset;
                    size_t const range_count = std::get<0>(*it_adds).count();
                    component_it = components.insert(component_it, range_count, std::move(std::get<1>(*it_adds)));
                    component_it = std::next(component_it, range_count);
                };
                auto const insert_init = [&](size_t offset) {
                    // Add the new data
                    component_it += offset;
                    auto const& range = std::get<0>(*it_inits);
                    auto const& init = std::get<1>(*it_inits);
                    for (entity_id const ent : range) {
                        component_it = components.emplace(component_it, init(ent));
                        component_it = std::next(component_it);
                    }
                };

                while (it_adds != adds.end() && it_inits != inits.end()) {
                    if (std::get<0>(*it_adds) < std::get<0>(*it_inits)) {
                        insert_data(insert_range(it_adds));
                        ++it_adds;
                    } else {
                        insert_init(insert_range(it_inits));
                        ++it_inits;
                    }
                }

                while (it_adds != adds.end()) {
                    insert_data(insert_range(it_adds));
                    ++it_adds;
                }
                while (it_inits != inits.end()) {
                    insert_init(insert_range(it_inits));
                    ++it_inits;
                }
            } else {
                // If there is no data, the ranges are always added to 'deferred_adds'
                while (it_adds != adds.end()) {
                    insert_range(it_adds);
                    ++it_adds;
                }
            }

            // Move the remaining ranges
            std::move(ranges_it, ranges.cend(), std::back_inserter(new_ranges));

            // Store the new ranges
            ranges = std::move(new_ranges);

            // Update the state
            set_data_added();
        }

        // Removes the entities and components
        void process_remove_components() {
            // Transient components are removed each cycle
            if constexpr (detail::transient<T>) {
                if (!ranges.empty()) {
                    ranges.clear();
                    components.clear();
                    set_data_removed();
                }
            } else {
                // Combine the vectors
                std::vector<entity_range> removes;
                for (auto& vec : deferred_removes) {
                    std::move(vec.begin(), vec.end(), std::back_inserter(removes));
                }

                if (removes.empty()) {
                    return;
                }

                // Clear the current removes
                deferred_removes.clear();

                // Sort it if needed
                if (!std::is_sorted(removes.begin(), removes.end()))
                    std::sort(removes.begin(), removes.end());

                // An entity can not have more than one of the same component
                auto const has_duplicate_entities = [](auto const& vec) {
                    return vec.end() != std::adjacent_find(vec.begin(), vec.end());
                };
                Expects(false == has_duplicate_entities(removes));

                // Merge adjacent ranges
                auto const combiner = [](auto& a, auto const& b) {
                    if (a.can_merge(b)) {
                        a = entity_range::merge(a, b);
                        return true;
                    } else {
                        return false;
                    }
                };
                combine_erase(removes, combiner);

                // Remove the components
                if constexpr (!unbound<T>) {
                    // Find the first valid index
                    auto index = find_entity_index(removes.front().first());
                    Expects(index.has_value());
                    auto dest_it = components.begin() + index.value();
                    auto from_it = dest_it + removes.front().count();

                    if (dest_it == components.begin() && from_it == components.end()) {
                        components.clear();
                    } else {
                        // Move components inbetween the ranges
                        for (auto it = removes.cbegin() + 1; it != removes.cend(); ++it) {
                            index = find_entity_index(it->first());

                            auto const last_it = components.begin() + index.value();
                            auto const dist = std::distance(from_it, last_it);
                            from_it = std::move(from_it, last_it, dest_it);
                            dest_it += dist;
                        }

                        // Move rest of components
                        auto const dist = std::distance(from_it, components.end());
                        std::move(from_it, components.end(), dest_it);

                        // Erase the unused space
                        if (dest_it + dist != components.end()) {
                            components.erase(dest_it + dist, components.end());
                        } else {
                            components.erase(dest_it, components.end());
                        }
                    }
                }

                // Remove the ranges
                auto curr_range = ranges.begin();
                for (auto const& remove : removes) {
                    // Step forward until a candidate range is found
                    while (*curr_range < remove && curr_range != ranges.end()) {
                        ++curr_range;
                    }

                    if (curr_range == ranges.end()) {
                        break;
                    }

                    Expects(curr_range->contains(remove));

                    // Erase the current range if it equals the range to be removed
                    if (curr_range->equals(remove)) {
                        curr_range = ranges.erase(curr_range);
                    } else {
                        // Do the removal
                        auto result = entity_range::remove(*curr_range, remove);

                        // Update the modified range
                        *curr_range = result.first;

                        // If the range was split, add the other part of the range
                        if (result.second.has_value()) {
                            curr_range = ranges.insert(curr_range + 1, result.second.value());
                        }
                    }
                }

                // Update the state
                set_data_removed();
            }
        }
    };
} // namespace ecs::detail

#endif // !__COMPONENT_POOL

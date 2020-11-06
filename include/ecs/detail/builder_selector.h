#ifndef __BUILDER_SELECTOR_H
#define __BUILDER_SELECTOR_H

#include "builder_ranged_argument.h"
#include "builder_sorted_argument.h"
#include "builder_hierachy_argument.h"

namespace ecs::detail {

    // Chooses an argument builder and returns a nullptr to it
    template<typename Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    constexpr auto get_ptr_builder() {
        bool constexpr has_sort_func = !std::is_same_v<SortFn, std::nullptr_t>;
        bool constexpr has_parent = std::is_same_v<std::remove_cvref_t<FirstComponent>, ecs::parent> ||
                                    (std::is_same_v<std::remove_cvref_t<Components>, ecs::parent> || ...);

        static_assert(!(has_sort_func == has_parent && has_parent == true),
            "Systems can not both be hierarchial and sorted");

        if constexpr (has_sort_func) {
            return (builder_sorted_argument<Options, UpdateFn, SortFn, FirstComponent, Components...>*) nullptr;
        } else if constexpr (has_parent) {
            return (builder_hierarchy_argument<Options, UpdateFn, SortFn, FirstComponent, Components...>*) nullptr;
        } else {
            return (builder_ranged_argument<Options, UpdateFn, SortFn, FirstComponent, Components...>*) nullptr;
        }
    }

    template<typename Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    using builder_selector =
        std::remove_pointer_t<decltype(get_ptr_builder<Options, UpdateFn, SortFn, FirstComponent, Components...>())>;
} // namespace ecs::detail

#endif // !__BUILDER_SELECTOR_H

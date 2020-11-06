#ifndef __BUILDER_SELECTOR_H
#define __BUILDER_SELECTOR_H

namespace ecs::detail {

    // Chooses an argument builder and returns a nullptr to it
    template<typename Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    constexpr auto get_ptr_builder() {
        bool constexpr has_sort_func = !std::is_same_v<SortFn, std::nullptr_t>;

        if constexpr (has_sort_func) {
            return (builder_sorted_argument<Options, UpdateFn, SortFn, FirstComponent, Components...>*) nullptr;
        } else {
            return (builder_ranged_argument<Options, UpdateFn, SortFn, FirstComponent, Components...>*) nullptr;
        }
    }

    template<typename Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    using builder_selector =
        std::remove_pointer_t<decltype(get_ptr_builder<Options, UpdateFn, SortFn, FirstComponent, Components...>())>;
} // namespace ecs::detail

#endif // !__BUILDER_SELECTOR_H

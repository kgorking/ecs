#pragma once

namespace ecs::opts {
    template<size_t I>
    struct group {
        static constexpr size_t group_id = I;
    };

    template<size_t I>
    struct interval {
        static constexpr size_t hz = I;
    };

    struct manual_update {};

    struct never_concurrent {};

} // namespace ecs::opts

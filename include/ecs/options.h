#pragma once

namespace ecs::opts {
    template<int I>
    struct group {
        static constexpr int group_id = I;
    };

    template<size_t I>
    struct interval {
        static constexpr size_t hz = I;
    };

    struct manual_update {};

    struct never_concurrent {};

} // namespace ecs::opts

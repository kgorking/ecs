#ifndef ECS_OPTIONS_H
#define ECS_OPTIONS_H

namespace ecs::opts {
    template<int I>
    struct group {
        static constexpr int group_id = I;
    };

    template<size_t I>
    struct frequency {
        static constexpr size_t hz = I;
    };

    struct manual_update {};

    struct not_parallel {};
    //struct not_concurrent {};

} // namespace ecs::opts

#endif // !ECS_OPTIONS_H

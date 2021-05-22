#ifndef ECS_OPTIONS_H
#define ECS_OPTIONS_H

namespace ecs::opts {
    template<int I>
    struct group {
        static constexpr int group_id = I;
    };

    template<double Milliseconds>
    struct interval {
        static constexpr double _ecs_duration = Milliseconds;
    };

    struct manual_update {};

    struct not_parallel {};
    //struct not_concurrent {};

} // namespace ecs::opts

#endif // !ECS_OPTIONS_H

#ifndef ECS_OPTIONS_H
#define ECS_OPTIONS_H

namespace ecs::opts {
    template<int I>
    struct group {
        static constexpr int group_id = I;
    };

    // Sets a fixed execution frequency for a system.
    // The system will be run atleast 1.0/Hertz times per second.
    template<size_t Hertz>
    struct frequency {
        static constexpr size_t hz = Hertz;
    };

    template<typename Duration>
    struct interval {
        static constexpr Duration duration{};
    };

    struct manual_update {};

    struct not_parallel {};
    //struct not_concurrent {};

} // namespace ecs::opts

#endif // !ECS_OPTIONS_H

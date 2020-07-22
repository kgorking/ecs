#pragma once
#include <execution>

namespace ecs::detail {
    // Contains detectors for the options


    //
    // Check if type is a group
    template<typename T>
    concept is_group = requires {
        T::group_id;
    };


    //
    // Check if the options has an execution policy
    template<typename... Options>
    concept has_execution_policy = (std::is_execution_policy_v<Options> || ...);

    template<typename... Options>
    using execution_policy = std::condition_t<has_execution_policy<Options...>,
        std::execution::parallel_unsequenced_policy, sequenced_policy>;

} // namespace ecs::detail

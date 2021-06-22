#ifndef ECS_OPTIONS_H
#define ECS_OPTIONS_H

namespace ecs::opts {
template <int I>
struct group {
	static constexpr int group_id = I;
};

template <int Milliseconds, int Microseconds = 0>
struct interval {
	static_assert(Milliseconds >= 0, "invalid time values specified");
	static_assert(Microseconds >= 0 && Microseconds < 1000, "invalid time values specified");

	static constexpr double _ecs_duration = (1.0 * Milliseconds) + (Microseconds / 1000.0);
	static constexpr int _ecs_duration_ms = Milliseconds;
	static constexpr int _ecs_duration_us = Microseconds;
};

struct manual_update {};

struct not_parallel {};

// sched_ignore_cache_temporality

} // namespace ecs::opts

#endif // !ECS_OPTIONS_H

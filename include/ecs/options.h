#ifndef ECS_OPTIONS_H
#define ECS_OPTIONS_H

ECS_EXPORT namespace ecs::opts {
	template <int Milliseconds, int Microseconds = 0>
	struct interval {
		static_assert(Milliseconds >= 0, "time values can not be negative");
		static_assert(Microseconds >= 0, "time values can not be negative");
		static_assert(Microseconds <= 999, "microseconds must be in the range 0-999");

		static constexpr int ms = Milliseconds;
		static constexpr int us = Microseconds;
	};

	struct manual_update {};

	struct not_parallel {};
	// struct not_concurrent {};

} // namespace ecs::opts

#endif // !ECS_OPTIONS_H

#ifndef ECS_FREQLIMIT_H
#define ECS_FREQLIMIT_H

#include <chrono>

namespace ecs::detail {

template <int milliseconds, int microseconds>
struct interval_limiter {
	bool can_run() {
		using namespace std::chrono_literals;
		static constexpr std::chrono::nanoseconds interval_size = 1ms * milliseconds + 1us * microseconds;

		auto const now = std::chrono::high_resolution_clock::now();
		auto const diff = now - time;
		if (diff >= interval_size) {
			time = now;
			return true;
		} else {
			return false;
		}
	}

private:
	std::chrono::high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now();
};

template <>
struct interval_limiter<0, 0> {
	static constexpr bool can_run() {
		return true;
	}
};

} // namespace ecs::detail

#endif // !ECS_FREQLIMIT_H

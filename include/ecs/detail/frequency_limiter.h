#ifndef __FREQLIMIT_H
#define __FREQLIMIT_H

#include <chrono>

namespace ecs::detail {
    // microsecond precision
	template<size_t hz>
	struct frequency_limiter {
        using clock = std::chrono::high_resolution_clock;

        bool can_run() {
            using namespace std::chrono_literals;

            if constexpr (hz == 0)
                return true;
            else {
                auto const now = clock::now();
                auto const diff = now - time;
                if (diff >= (1'000'000us / hz)) {
                    time = now;
                    return true;
                } else {
                    return false;
                }
            }
		}

	private:
        clock::time_point time = clock::now();
	};
}

#endif // !__FREQLIMIT_H

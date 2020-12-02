#ifndef __FREQLIMIT_H
#define __FREQLIMIT_H

#include <chrono>

namespace ecs::detail {

	template<size_t hz>
	struct frequency_limiter {
        bool can_run() {
            if constexpr (hz == 0)
                return true;
            else {
                using namespace std::chrono_literals;

                auto const now = std::chrono::high_resolution_clock::now();
                auto const diff = now - time;
                if (diff >= (1'000'000'000ns / hz)) {
                    time = now;
                    return true;
                } else {
                    return false;
                }
            }
		}

    private:
        std::chrono::high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now();
    };
}

#endif // !__FREQLIMIT_H

#ifndef ECS_DETAIL_STRIDE_VIEW_H
#define ECS_DETAIL_STRIDE_VIEW_H

#include "contract.h"

namespace ecs::detail {

// 
template <std::size_t Stride, typename T>
class stride_view {
	char const* first{nullptr};
	char const* curr{nullptr};
	char const* last{nullptr};

public:
	stride_view() noexcept = default;
	stride_view(T const* first_, std::size_t count_) noexcept
		: first{reinterpret_cast<char const*>(first_)}
		, curr {reinterpret_cast<char const*>(first_)}
		, last {reinterpret_cast<char const*>(first_) + Stride*count_} {
		Pre(first_ != nullptr, "input pointer can not be null");
	}

	T const* current() const noexcept {
		return reinterpret_cast<T const*>(curr);
	}

	bool done() const noexcept {
		return (first==nullptr) || (curr >= last);
	}

	void next() noexcept {
		if (!done())
			curr += Stride;
	}
};

}

#endif //!ECS_DETAIL_STRIDE_VIEW_H

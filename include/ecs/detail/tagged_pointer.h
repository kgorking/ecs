#ifndef ECS_DETAIL_TAGGED_POINTER_H
#define ECS_DETAIL_TAGGED_POINTER_H

#include <bit>
#include <cstdint>

namespace ecs::detail {

// 1-bit tagged pointer
// Note: tags are considered seperate from the pointer, and is
// therfore not reset when a new pointer is set
template <typename T>
struct tagged_pointer {
	constexpr tagged_pointer(T* in) noexcept : ptr(std::bit_cast<uintptr_t>(in)) {}

	constexpr tagged_pointer() noexcept = default;
	constexpr tagged_pointer(tagged_pointer const&) noexcept = default;
	constexpr tagged_pointer(tagged_pointer&&) noexcept = default;
	constexpr tagged_pointer& operator=(tagged_pointer const&) noexcept = default;
	constexpr tagged_pointer& operator=(tagged_pointer&&) noexcept = default;

	constexpr tagged_pointer& operator=(T* in) noexcept {
		bool const set = tag();
		ptr = std::bit_cast<uintptr_t>(in);
		set_tag(set);
		return *this;
	}

	tagged_pointer* self() {
		return this;
	}

	constexpr bool tag() const noexcept {
		return ptr & TagMask;
	}

	constexpr void set_tag(bool set) noexcept {
		if (set)
			ptr = ptr | 0x01ull;
		else
			ptr = ptr & PointerMask;
	}

	constexpr T* pointer() noexcept {
		return std::bit_cast<T*>(ptr & PointerMask);
	}
	constexpr T const* pointer() const noexcept {
		return std::bit_cast<T*>(ptr & PointerMask);
	}

	constexpr T* operator->() noexcept {
		return pointer();
	}
	constexpr T const* operator->() const noexcept {
		return pointer();
	}

	constexpr operator T*() noexcept {
		return pointer();
	}
	constexpr operator T const *() const noexcept {
		return pointer();
	}

private:
	constexpr static uintptr_t TagMask = 0x01ull;
	constexpr static uintptr_t PointerMask = ~0x01ull;

	uintptr_t ptr;
};
static_assert(sizeof(tagged_pointer<char>) == sizeof(char*));

} // namespace ecs::detail

#endif // ECS_DETAIL_TAGGED_POINTER_H
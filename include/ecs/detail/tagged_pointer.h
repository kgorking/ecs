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
	constexpr tagged_pointer(T* in) noexcept : val(std::bit_cast<uintptr_t>(in)) {}

	constexpr tagged_pointer() noexcept = default;
	constexpr tagged_pointer(tagged_pointer const&) noexcept = default;
	constexpr tagged_pointer(tagged_pointer&&) noexcept = default;
	constexpr tagged_pointer& operator=(tagged_pointer const&) noexcept = default;
	constexpr tagged_pointer& operator=(tagged_pointer&&) noexcept = default;

	constexpr tagged_pointer& operator=(T* in) noexcept {
		bool const set = tag();
		val = std::bit_cast<uintptr_t>(in);
		set_tag(set);
		return *this;
	}

	constexpr bool tag() const noexcept {
		return val & TagMask;
	}

	constexpr void set_tag(bool set) noexcept {
		if (set)
			val = val | 0x01ull;
		else
			val = val & PointerMask;
	}

	constexpr T* value() noexcept {
		return std::bit_cast<T*>(val & PointerMask);
	}
	constexpr T const* value() const noexcept {
		return std::bit_cast<T*>(val & PointerMask);
	}

	constexpr T* operator->() noexcept {
		return value();
	}
	constexpr T const* operator->() const noexcept {
		return value();
	}

	constexpr operator T*() noexcept {
		return value();
	}
	constexpr operator T const *() const noexcept {
		return value();
	}

private:
	constexpr static uintptr_t TagMask = 0x01ull;
	constexpr static uintptr_t PointerMask = ~0x01ull;

	uintptr_t val;
};
static_assert(sizeof(tagged_pointer<char>) == sizeof(char*));

} // namespace ecs::detail

#endif // ECS_DETAIL_TAGGED_POINTER_H
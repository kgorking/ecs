#ifndef ECS_DETAIL_TAGGED_POINTER_H
#define ECS_DETAIL_TAGGED_POINTER_H

#include <bit>
#include <cstdint>
#include "contract.h"

namespace ecs::detail {

// 1-bit tagged pointer
// Note: tags are considered seperate from the pointer, and is
// therfore not reset when a new pointer is set
template <typename T>
struct tagged_pointer {
	tagged_pointer(T* in) noexcept : ptr(reinterpret_cast<uintptr_t>(in)) {
		Expects((ptr & TagMask) == 0);
	}

	tagged_pointer() noexcept = default;
	tagged_pointer(tagged_pointer const&) noexcept = default;
	tagged_pointer(tagged_pointer&&) noexcept = default;
	tagged_pointer& operator=(tagged_pointer const&) noexcept = default;
	tagged_pointer& operator=(tagged_pointer&&) noexcept = default;

	tagged_pointer& operator=(T* in) noexcept {
		auto const set = ptr & TagMask;
		ptr = set | reinterpret_cast<uintptr_t>(in);
		return *this;
	}

	void clear() noexcept {
		ptr = 0;
	}
	void clear_bits() noexcept {
		ptr = ptr & PointerMask;
	}
	int get_tag() const noexcept {
		return ptr & TagMask;
	}
	void set_tag(int tag) noexcept {
		Expects(tag >= 0 && tag <= static_cast<int>(TagMask));
		ptr = (ptr & PointerMask) | static_cast<uintptr_t>(tag);
	}

	bool test_bit1() const noexcept
		requires(sizeof(void*) >= 2) {
		return ptr & static_cast<uintptr_t>(0b001);
	}
	bool test_bit2() const noexcept
		requires(sizeof(void*) >= 4) {
		return ptr & static_cast<uintptr_t>(0b010);
	}
	bool test_bit3() const noexcept
		requires(sizeof(void*) >= 8) {
		return ptr & static_cast<uintptr_t>(0b100);
	}

	void set_bit1() noexcept
		requires(sizeof(void*) >= 2) {
		ptr |= static_cast<uintptr_t>(0b001);
	}
	void set_bit2() noexcept
		requires(sizeof(void*) >= 4) {
		ptr |= static_cast<uintptr_t>(0b010);
	}
	void set_bit3() noexcept
		requires(sizeof(void*) >= 8) {
		ptr |= static_cast<uintptr_t>(0b100);
	}

	void clear_bit1() noexcept
		requires(sizeof(void*) >= 2) {
		ptr = ptr & ~static_cast<uintptr_t>(0b001);
	}
	void clear_bit2() noexcept
		requires(sizeof(void*) >= 4) {
		ptr = ptr & ~static_cast<uintptr_t>(0b010);
	}
	void clear_bit3() noexcept
		requires(sizeof(void*) >= 8) {
		ptr = ptr & ~static_cast<uintptr_t>(0b100);
	}

	T* pointer() noexcept {
		return reinterpret_cast<T*>(ptr & PointerMask);
	}
	T const* pointer() const noexcept {
		return reinterpret_cast<T*>(ptr & PointerMask);
	}

	T* operator->() noexcept {
		return pointer();
	}
	T const* operator->() const noexcept {
		return pointer();
	}

	operator T*() noexcept {
		return pointer();
	}
	operator T const *() const noexcept {
		return pointer();
	}

private:
	//constexpr static uintptr_t TagMask = 0b111;
	constexpr static uintptr_t TagMask = sizeof(void*) - 1;
	constexpr static uintptr_t PointerMask = ~TagMask;

	uintptr_t ptr;
};

} // namespace ecs::detail

#endif // ECS_DETAIL_TAGGED_POINTER_H
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
	tagged_pointer(T* in) : val(std::bit_cast<uintptr_t>(in)) {}

	tagged_pointer() = default;
	tagged_pointer(tagged_pointer const&) = default;
	tagged_pointer(tagged_pointer&&) = default;
	tagged_pointer& operator=(tagged_pointer const&) = default;
	tagged_pointer& operator=(tagged_pointer&&) = default;

	tagged_pointer& operator=(T* in) {
		bool const set = tag();
		val = std::bit_cast<uintptr_t>(in);
		set_tag(set);
		return *this;
	}

	bool tag() const {
		return val & 0x01ull;
	}

	void set_tag(bool set) {
		if (set)
			val |= 0x01ull;
		else
			val &= Mask;
	}

	T* value() {
		return std::bit_cast<T*>(val & Mask);
	}
	T const* value() const {
		return std::bit_cast<T*>(val & Mask);
	}

	T* operator->() {
		return value();
	}
	T const* operator->() const {
		return value();
	}

	operator T*() {
		return value();
	}
	operator T const *() const {
		return value();
	}

private:
	constexpr static uintptr_t Mask = ~0x01ULL;

	uintptr_t val;
};
static_assert(sizeof(tagged_pointer<char>) == sizeof(char*));

} // namespace ecs::detail

#endif // ECS_DETAIL_TAGGED_POINTER_H
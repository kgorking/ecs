#ifndef ECS_DETAIL_VARIANT_H
#define ECS_DETAIL_VARIANT_H

#include <concepts>

namespace ecs::detail {
	template <typename T>
	concept is_variant = requires { T::variant_of(); };

	template <typename A, typename B>
	constexpr bool is_variant_of() {
		if constexpr (!is_variant<A> && !is_variant<B>) {
			return false;
		}

		if constexpr (is_variant<A>) {
			static_assert(!std::same_as<A, typename A::variant_of>, "Types can not be variant with themselves");
			if (std::same_as<typename A::variant_of, B>)
				return true;
			if (is_variant<typename A::variant_of>)
				return is_variant_of<typename A::variant_of, B>();
		}

		if constexpr (is_variant<B>) {
			static_assert(!std::same_as<B, typename B::variant_of>, "Types can not be variant with themselves");
			if (std::same_as<typename B::variant_of, A>)
				return true;
			if (is_variant<typename B::variant_of>)
				return is_variant_of<typename B::variant_of, A>();
		}

		return false;
	};

	// Returns false if any types passed are variants of each other
	template <typename First, typename... T>
	constexpr bool is_variant_of_pack() {
		if constexpr ((is_variant_of<First, T>() || ...))
			return true;
		else {
			if constexpr (sizeof...(T) > 1)
				return is_variant_of_pack<T...>();
			else
				return false;
		}
	}

	template <is_variant T>
	using variant_t = typename T::variant_of;
} // namespace ecs::detail

#endif

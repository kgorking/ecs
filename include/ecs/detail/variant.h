#ifndef ECS_DETAIL_VARIANT_H
#define ECS_DETAIL_VARIANT_H

#include <concepts>

namespace ecs::detail {
	template <typename T>
	concept has_variant_alias = requires { typename T::variant_of; };

	template <has_variant_alias T>
	using variant_t = typename T::variant_of;

	template <typename A, typename B>
	consteval bool is_variant_of() {
		if constexpr (!has_variant_alias<A> && !has_variant_alias<B>) {
			return false;
		}

		if constexpr (has_variant_alias<A>) {
			static_assert(!std::same_as<A, typename A::variant_of>, "Types can not be variant with themselves");
			if (std::same_as<typename A::variant_of, B>)
				return true;
			if (has_variant_alias<typename A::variant_of>)
				return is_variant_of<typename A::variant_of, B>();
		}

		if constexpr (has_variant_alias<B>) {
			static_assert(!std::same_as<B, typename B::variant_of>, "Types can not be variant with themselves");
			if (std::same_as<typename B::variant_of, A>)
				return true;
			if (has_variant_alias<typename B::variant_of>)
				return is_variant_of<typename B::variant_of, A>();
		}

		return false;
	};

	// Returns false if any types passed are variants of each other
	template <typename First, typename... T>
	consteval bool is_variant_of_pack() {
		if constexpr ((is_variant_of<First, T>() || ...))
			return true;
		else {
			if constexpr (sizeof...(T) > 1)
				return is_variant_of_pack<T...>();
			else
				return false;
		}
	}

	template<typename T, typename V>
	consteval bool check_for_recursive_variant() {
		if constexpr (std::same_as<T, V>)
			return true;
		else if constexpr (!has_variant_alias<V>)
			return false;
		else
			return check_for_recursive_variant<T, variant_t<V>>();
	}

	template <has_variant_alias T>
	consteval bool not_recursive_variant() {
		return !check_for_recursive_variant<T, variant_t<T>>();
	}
	template <typename T>
	consteval bool not_recursive_variant() {
		return true;
	}
} // namespace ecs::detail

#endif

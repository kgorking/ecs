#ifndef ECS_VARIANT_H
#define ECS_VARIANT_H

#include "detail/type_list.h"

namespace ecs {
	namespace detail {
		template <typename T>
		concept is_variant = requires { std::type_identity_t<typename T::variant_of>{}; };

		template <typename A, typename B>
		constexpr bool is_variant_of() {
			if constexpr (!is_variant<A> && !is_variant<B>) {
				return false;
			}

			if constexpr (is_variant<A>) {
				static_assert(!std::is_same_v<A, typename A::variant_of>, "Types can not be variant with themselves");
				if (std::is_same_v<typename A::variant_of, B>)
					return true;
				if (is_variant<typename A::variant_of>)
					return is_variant_of<typename A::variant_of, B>();
			}

			if constexpr (is_variant<B>) {
				static_assert(!std::is_same_v<B, typename B::variant_of>, "Types can not be variant with themselves");
				if (std::is_same_v<typename B::variant_of, A>)
					return true;
				if (is_variant<typename B::variant_of>)
					return is_variant_of<typename B::variant_of, A>();
			}

			return false;
		};
	} // namespace detail
} // namespace ecs

#endif

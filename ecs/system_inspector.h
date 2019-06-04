#pragma once

namespace ecs::detail
{
	// Check that we are only supplied unique types - https://stackoverflow.com/questions/18986560/check-variadic-templates-parameters-for-uniqueness
	template <typename...> struct is_one_of;
	template <typename F>  struct is_one_of<F> { static constexpr bool value = false; };
	template <typename F, typename S, typename... T> struct is_one_of<F, S, T...> { static constexpr bool value = std::is_same<F, S>::value || is_one_of<F, T...>::value; };

	template <typename...> struct is_unique;
	template <>            struct is_unique<> { static constexpr bool value = true; };
	template<typename F, typename... T> struct is_unique<F, T...> { static constexpr bool value = is_unique<T...>::value && !is_one_of<F, T...>::value; };

	// Based on https://stackoverflow.com/questions/7943525/is-it-possible-to-figure-out-the-parameter-type-and-return-type-of-a-lambda

	// A class to strip a lambda to its basic components
	template <typename T>
	struct system_inspector : public system_inspector<decltype(&T::operator())> {};

	template <typename C, typename R, typename... Args>
	struct system_inspector<R(C::*)(Args...) const>
	{
	public:
		static constexpr size_t num_args = sizeof...(Args);
		using argument_types = std::tuple<Args...>;
		using return_type = R;

		// Get the type of a specific argument
		template <size_t I>
		using arg_at = typename std::tuple_element<I, argument_types>::type;

		// Returns true if the same component has been specified more than once
		constexpr static bool has_unique_components() {
			return detail::is_unique<Args...>::value;
		}

		// Returns true if all the components are passed by reference (const or not)
		constexpr static bool components_passed_by_ref() { return is_reference(std::make_index_sequence<sizeof...(Args) - 1> {}); }

	private:
		// Check that all components are references
		template<size_t ...I>
		constexpr static bool is_reference(std::index_sequence<I...>) {
			return
				(std::is_reference_v<arg_at<1 + I>> && ...);
		}
	};
}

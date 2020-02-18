#pragma once
#include <utility>

namespace ecs {
	struct entity_id;
	class entity;
}

namespace ecs::detail
{
	// Check that we are only supplied unique types - https://stackoverflow.com/questions/18986560/check-variadic-templates-parameters-for-uniqueness
	template <typename...> struct is_one_of;
	template <typename F>  struct is_one_of<F> { static constexpr bool value = false; };
	template <typename F, typename S, typename... T> struct is_one_of<F, S, T...> { static constexpr bool value = std::is_same<F, S>::value || is_one_of<F, T...>::value; };

	template <typename...> struct is_unique;
	template <>            struct is_unique<> { static constexpr bool value = true; };
	template<typename F, typename... T> struct is_unique<F, T...> { static constexpr bool value = is_unique<T...>::value && !is_one_of<F, T...>::value; };

	// detection idiom
	template <class T, class = void>  struct is_lambda : std::false_type {};
	template <class T>                struct is_lambda<T, std::void_t<decltype(&T::operator ())>> : std::true_type {};
	template <class T> static constexpr bool is_lambda_v = is_lambda<T>::value;

	template <class T>
	using naked_type = std::remove_pointer_t<std::remove_reference_t<std::remove_cv_t<T>>>;

	// A class to verify a system is valid
	// Based on https://stackoverflow.com/questions/7943525/is-it-possible-to-figure-out-the-parameter-type-and-return-type-of-a-lambda
	template <typename C, typename R, typename... Args>
	struct system_inspector_impl
	{
	public:
		static constexpr size_t num_args = sizeof...(Args);
		using argument_types = std::tuple<Args...>;
		using return_type = R;

		// Get the type of a specific argument
		template <size_t I>
		using arg_at = typename std::tuple_element<I, argument_types>::type;

		using first_type = arg_at<0>;
		using naked_first_type = std::remove_pointer_t<std::remove_reference_t<std::remove_cv_t<first_type>>>;

		static bool constexpr has_entity_id = std::is_same_v<naked_first_type, entity_id>;
		static bool constexpr has_entity_struct = std::is_same_v<naked_first_type, entity>;
		static bool constexpr has_entity = has_entity_id || has_entity_struct;

		// False if the same component has been specified more than once
		static bool constexpr has_unique_components = detail::is_unique<Args...>::value;

		// Returns true if all the components are passed by reference (const or not)
		constexpr static bool components_passed_by_ref() {
			// If the systems first argument is an entity, then there is one less component to check
			constexpr int sizeof_adjust = has_entity ? 1 : 0;
			return is_reference(std::make_index_sequence<sizeof...(Args) - sizeof_adjust> {});
		}

		// Returns true if none of the components voilate the 'unmutable' flag
		constexpr static bool components_honors_mutability() {
			return (!violates_unmutable<Args>() && ...);
		}

	private:
		// Check that all components are references
		template<size_t ...I>
		constexpr static bool is_reference(std::index_sequence<I...> /*is*/) {
			if constexpr (!has_entity)
				return (std::is_reference_v<arg_at<I>> && ...);
			else
				return (std::is_reference_v<arg_at<1 + I>> && ...);
		}

		// Returns true if a component flagged as unmutable is not const
		template <typename T>
		constexpr static bool violates_unmutable() {
			return detail::is_unmutable_v<naked_type<T>> && !std::is_const_v<T>;
		}
	};

	// Splits a lambda into its various type parts (return value, class, function arguments)
	template <typename T>
	struct system_inspector : public system_inspector<decltype(&T::operator())>
	{ };

	// For immutable lambdas
	template <typename C, typename R, typename... Args>
	struct system_inspector<R(C::*)(Args...) const> : public system_inspector_impl<C, R, Args...>
	{ };

	// For mutable lambdas
	template <typename C, typename R, typename... Args>
	struct system_inspector<R(C::*)(Args...) /* */> : public system_inspector_impl<C, R, Args...>
	{ };


	template <typename System>
	constexpr void verify_system()
	{
		static_assert(is_lambda_v<System>, "system must be a valid lambda or function object");

		using inspector = system_inspector<System>;

		//
		// Make sure any entity types are not passed as references or pointers
		if constexpr (inspector::has_entity) {
			static_assert(!std::is_pointer_v<typename inspector::first_type>, "entities are only passed by value; remove the '*'");
			static_assert(!std::is_reference_v<typename inspector::first_type>, "entities are only passed by value; remove the '&'");
		}

		//
		// Implement the rules for systems
		static_assert(std::is_same_v<typename inspector::return_type, void>, "systems can not return values");
		static_assert(inspector::num_args > (inspector::has_entity ? 1 : 0), "no component types specified for the system");

		//
		// Implement the rules for components
		static_assert(inspector::has_unique_components, "a component type was specifed more than once");
		static_assert(inspector::components_passed_by_ref(), "systems can only take references to components");
		static_assert(inspector::components_honors_mutability(), "a component flagged as 'unmutable' is not const");
	}
}

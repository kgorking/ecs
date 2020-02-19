#pragma once
#include <utility>
#include "component_specifier.h"

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

	template <class T>
	using naked_type = std::remove_pointer_t<std::remove_cvref_t<T>>;

	// A class to verify a system is valid
	// Based on https://stackoverflow.com/questions/7943525/is-it-possible-to-figure-out-the-parameter-type-and-return-type-of-a-lambda
	template <typename C, typename R, typename FirstArg, typename... Args>
	struct system_inspector_impl
	{
	public:
		constexpr static void verify() {
			verify_system_api();
			verify_first_arg_entity();
			verify_components_are_unique();
			verify_immutable_components();
			verify_component_qualifiers();
		}

	private:
		// Verify that the system isn't trying to return anything, and that it has a component signature.
		constexpr static void verify_system_api() {
			static_assert(std::is_same_v<return_type, void>, "systems can not return values");
			static_assert(num_args > (has_entity ? 1 : 0), "no component specified for the system");
		}

		// Verify that the entity is correctly qualified, if the first argument is an entity
		constexpr static void verify_first_arg_entity() {
			// Make sure entity types are not passed as references or pointers
			if constexpr (has_entity) {
				static_assert(!std::is_pointer_v<FirstArg>, "entities are only passed by value; remove the '*'");
				static_assert(!std::is_reference_v<FirstArg>, "entities are only passed by value; remove the '&'");
			}
		}

		// Verify that all of the components are only used once
		constexpr static void verify_components_are_unique() {
			static_assert(detail::is_unique<FirstArg, Args...>::value, "a component type was specifed more than once");
		}

		// Verify that components flagged as 'unmutable' are also marked as const in the system
		constexpr static void verify_immutable_components() {
			if constexpr (!has_entity)
				verify_immutable<FirstArg>();
			(verify_immutable<Args>(), ...);
		}

		// Verify that a component flagged as unmutable is also const
		template <typename T>
		constexpr static void verify_immutable() {
			if constexpr (detail::immutable<naked_type<T>>) {
				static_assert(std::is_const_v<std::remove_reference_t<T>>, "a non-const component is flagged as 'immutable'");
			}
		}


		// Verify that components have the correct qualifiers.
		// All but 'tag' components are required to be references
		constexpr static void verify_component_qualifiers() {
			if constexpr (!has_entity)
				verify_qualifiers<FirstArg>();
			(verify_qualifiers<Args>(), ...);
		}

		// Verify that a component has correct qualifiers.
		template <class T>
		constexpr static void verify_qualifiers() {
			if constexpr (tagged<naked_type<T>>) {
				static_assert(sizeof(T) <= 1, "tag-components should not have any data in them");
				static_assert(!std::is_reference_v<T>, "components flagged as 'tag' can not be references, as they hold no data");
			}
			else
				static_assert(std::is_reference_v<T>, "components must be references");
		}

	private:
		using return_type = R;
		using naked_first_type = naked_type<FirstArg>;

		static bool constexpr has_entity_id = std::is_same_v<naked_first_type, entity_id>;
		static bool constexpr has_entity_struct = std::is_same_v<naked_first_type, entity>;
		static bool constexpr has_entity = has_entity_id || has_entity_struct;

		static constexpr size_t num_args = 1 + sizeof...(Args); // +1 for FirstArg
	};

	// Splits a lambda into its various type parts (return value, class, function arguments)
	template <typename T>
	struct system_inspector : public system_inspector<decltype(&T::operator())>
	{ };

	// The different types of lambdas
	template <typename C, typename R, typename... Args> struct system_inspector<R(C::*)(Args...)>                : public system_inspector_impl<C, R, Args...> {};
	template <typename C, typename R, typename... Args> struct system_inspector<R(C::*)(Args...) noexcept>       : public system_inspector_impl<C, R, Args...> {};
	template <typename C, typename R, typename... Args> struct system_inspector<R(C::*)(Args...) const>          : public system_inspector_impl<C, R, Args...> {};
	template <typename C, typename R, typename... Args> struct system_inspector<R(C::*)(Args...) const noexcept> : public system_inspector_impl<C, R, Args...> {};

	template <typename System>
	constexpr void verify_system()
	{
		using inspector = system_inspector<System>;
		inspector::verify();
	}
}

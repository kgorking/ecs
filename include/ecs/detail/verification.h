#ifndef ECS_VERIFICATION_H
#define ECS_VERIFICATION_H

#include <concepts>
#include <type_traits>

#include "../entity_id.h"
#include "../flags.h"
#include "options.h"
#include "system_defs.h"

namespace ecs::detail {

// Given a type T, if it is callable with an entity argument,
// resolve to the return type of the callable. Otherwise assume the type T.
template <typename T>
using get_type_t = std::conditional_t<std::invocable<T, entity_type>,
	std::invoke_result<T, entity_type>,
	std::type_identity<T>>;


// Returns true if all types passed are unique
template <typename First, typename... T>
constexpr bool is_unique_types() {
	if constexpr ((std::is_same_v<First, T> || ...))
		return false;
	else {
		if constexpr (sizeof...(T) == 0)
			return true;
		else
			return is_unique_types<T...>();
	}
}


// Find the types a sorting predicate takes
template <typename R, typename T>
constexpr std::remove_cvref_t<T> get_sorter_type(R (*)(T, T)) { return T{}; }			// Standard function

template <typename R, typename C, typename T>
constexpr std::remove_cvref_t<T> get_sorter_type(R (C::*)(T, T) const) {return T{}; }	// const member function
template <typename R, typename C, typename T>
constexpr std::remove_cvref_t<T> get_sorter_type(R (C::*)(T, T) ) {return T{}; }			// mutable member function


template <typename Pred>
constexpr auto get_sorter_type() {
	// Verify predicate
	static_assert(
		requires {
			{ Pred{}({}, {}) } -> std::same_as<bool>;
		},
		"predicates must take two arguments and return a bool");

	if constexpr (requires { &Pred::operator(); }) {
		return get_sorter_type(&Pred::operator());
	} else {
		return get_sorter_type(Pred{});
	}
}

template <typename Pred>
using sorter_predicate_type_t = decltype(get_sorter_type<Pred>());


// Implement the requirements for ecs::parent components
template <typename C>
constexpr void verify_parent_component() {
	if constexpr (detail::is_parent<C>::value) {
		using parent_subtypes = parent_type_list_t<std::remove_cvref_t<C>>;
		constexpr size_t total_subtypes = type_list_size<parent_subtypes>;

		if constexpr (total_subtypes > 0) {
			// Count all the filters in the parent type
			constexpr size_t num_subtype_filters =
				apply_type<parent_subtypes>([]<typename... Types>() { return (std::is_pointer_v<Types> + ...); });

			// Count all the types minus filters in the parent type
			constexpr size_t num_parent_subtypes = total_subtypes - num_subtype_filters;

			// If there is one-or-more sub-components,
			// then the parent must be passed as a reference
			if constexpr (num_parent_subtypes > 0) {
				constexpr bool cref_or_value = !(std::is_reference_v<C> && !std::is_const_v<std::remove_reference_t<C>>);
				static_assert(cref_or_value, "parent with non-filter sub-components must be passed as a value or const&");
			}
		}
	}
}

// Implement the requirements for tagged components
template <typename C>
constexpr void verify_tagged_component() {
	if constexpr (detail::tagged<C>)
		static_assert(!std::is_reference_v<C> && (sizeof(C) == 1), "components flagged as 'tag' must not be references");
}

// Implement the requirements for global components
template <typename C>
constexpr void verify_global_component() {
	if constexpr (detail::global<C>)
		static_assert(!detail::tagged<C> && !detail::transient<C>, "components flagged as 'global' must not be 'tag's or 'transient'");
}

// Implement the requirements for immutable components
template <typename C>
constexpr void verify_immutable_component() {
	if constexpr (detail::immutable<C>)
		static_assert(std::is_const_v<std::remove_reference_t<C>>, "components flagged as 'immutable' must also be const");
}

template <typename R, typename FirstArg, typename... Args>
constexpr void system_verifier() {
	static_assert(std::is_same_v<R, void>, "systems can not have returnvalues");

	static_assert(is_unique_types<FirstArg, Args...>(), "component parameter types can only be specified once");

	if constexpr (is_entity<FirstArg>) {
		static_assert(sizeof...(Args) > 0, "systems must take at least one component argument");

		// Make sure the first entity is not passed as a reference
		static_assert(!std::is_reference_v<FirstArg>, "ecs::entity_id must not be passed as a reference");
	}

	verify_immutable_component<FirstArg>();
	(verify_immutable_component<Args>(), ...);

	verify_global_component<FirstArg>();
	(verify_global_component<Args>(), ...);

	verify_tagged_component<FirstArg>();
	(verify_tagged_component<Args>(), ...);

	verify_parent_component<FirstArg>();
	(verify_parent_component<Args>(), ...);
}

// A small bridge to allow the Lambda to activate the system verifier
template <typename R, typename C, typename FirstArg, typename... Args>
constexpr void system_to_lambda_bridge(R (C::*)(FirstArg, Args...)) { system_verifier<R, FirstArg, Args...>(); }
template <typename R, typename C, typename FirstArg, typename... Args>
constexpr void system_to_lambda_bridge(R (C::*)(FirstArg, Args...) const) { system_verifier<R, FirstArg, Args...>(); }
template <typename R, typename C, typename FirstArg, typename... Args>
constexpr void system_to_lambda_bridge(R (C::*)(FirstArg, Args...) noexcept) { system_verifier<R, FirstArg, Args...>(); }
template <typename R, typename C, typename FirstArg, typename... Args>
constexpr void system_to_lambda_bridge(R (C::*)(FirstArg, Args...) const noexcept) { system_verifier<R, FirstArg, Args...>(); }

// A small bridge to allow the function to activate the system verifier
template <typename R, typename FirstArg, typename... Args>
constexpr void system_to_func_bridge(R (*)(FirstArg, Args...)) { system_verifier<R, FirstArg, Args...>(); }
template <typename R, typename FirstArg, typename... Args>
constexpr void system_to_func_bridge(R (*)(FirstArg, Args...) noexcept) { system_verifier<R, FirstArg, Args...>(); }

template <typename T>
concept type_is_lambda = requires {
	&T::operator();
};

template <typename T>
concept type_is_function = requires(T t) {
	system_to_func_bridge(t);
};

template <typename OptionsTypeList, typename SystemFunc, typename SortFunc>
constexpr void make_system_parameter_verifier() {
	bool constexpr is_lambda = type_is_lambda<SystemFunc>;
	bool constexpr is_func = type_is_function<SystemFunc>;

	static_assert(is_lambda || is_func, "Systems can only be created from lambdas or free-standing functions");

	// verify the system function
	if constexpr (is_lambda) {
		system_to_lambda_bridge(&SystemFunc::operator());
	} else if constexpr (is_func) {
		system_to_func_bridge(SystemFunc{});
	}

	// verify the sort function
	if constexpr (!std::is_same_v<std::nullptr_t, SortFunc>) {
		bool constexpr is_sort_lambda = type_is_lambda<SortFunc>;
		bool constexpr is_sort_func = type_is_function<SortFunc>;

		static_assert(is_sort_lambda || is_sort_func, "invalid sorting function");

		using sort_types = sorter_predicate_type_t<SortFunc>;
		static_assert(std::predicate<SortFunc, sort_types, sort_types>, "Sorting function is not a predicate");
	}
}

} // namespace ecs::detail

#endif // !ECS_VERIFICATION_H

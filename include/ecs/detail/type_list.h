#ifndef TYPE_LIST_H_
#define TYPE_LIST_H_

namespace ecs::detail {

//
// Helper templates for working with types at compile-time

// inspired by https://devblogs.microsoft.com/cppblog/build-throughput-series-more-efficient-template-metaprogramming/

template <typename...>
struct type_list;

template <typename First, typename Second>
struct type_pair {
	using first = First;
	using second = Second;
};

namespace impl {
	//
	// detect type_list
	template <typename... Types>
	consteval bool detect_type_list(type_list<Types...>*) {
		return true;
	}
	consteval bool detect_type_list(...) {
		return false;
	}


	//
	// type_list_size.
	template <typename> struct type_list_size;
	template <typename... Types> struct type_list_size<type_list<Types...>> {
		static constexpr size_t value = sizeof...(Types);
	};


	//
	// type_list indices
	template<int Index, typename...>
	struct type_list_index {
		using type_not_found_in_list = decltype([]{});
		consteval static int index_of(type_not_found_in_list*);
	};

	template<int Index, typename T, typename... Rest>
	struct type_list_index<Index, T, Rest...> : type_list_index<1+Index, Rest...> {
		using type_list_index<1+Index, Rest...>::index_of;

		consteval static int index_of(T*) {
			return Index;
		}
	};

	template<typename... Types>
	auto type_list_indices(type_list<Types...>*) {
		struct all_indexers : impl::type_list_index<0, Types...> {
			using impl::type_list_index<0, Types...>::index_of;
		};
		return all_indexers{};
	};

	//
	// type_list concept
	template <class TL>
	concept TypeList = detect_type_list(static_cast<TL*>(nullptr));


	//
	// helper functions
	template <typename... Types, typename F>
	constexpr void for_each_type(F&& f, type_list<Types...>*) {
		(f.template operator()<Types>(), ...);
	}

	template <typename T, typename... Types, typename F>
	constexpr void for_specific_type(F&& f, type_list<Types...>*) {
		auto const runner = [&f]<typename X>() {
			if constexpr (std::is_same_v<T, X>) {
				f();
			}
		};

		(runner.template operator()<Types>(), ...);
	}

	template <typename T, typename... Types, typename F, typename NF>
	constexpr void for_specific_type_or(F&& f, NF&& nf, type_list<Types...>*) {
		auto const runner = [&]<typename X>() {
			if constexpr (std::is_same_v<T, X>) {
				f();
			} else {
				nf();
			}
		};

		(runner.template operator()<Types>(), ...);
	}

	template <typename... Types, typename F>
	constexpr decltype(auto) apply_type(F&& f, type_list<Types...>*) {
		return f.template operator()<Types...>();
	}

	template <typename... Types, typename F>
	constexpr bool all_of_type(F&& f, type_list<Types...>*) {
		return (f.template operator()<Types>() && ...);
	}

	template <typename... Types, typename F>
	constexpr bool any_of_type(F&& f, type_list<Types...>*) {
		return (f.template operator()<Types>() || ...);
	}

	template <template <class O> class Tester, typename... Types, typename F>
	constexpr auto run_if(F&& /*f*/, type_list<>*) {
		return;
	}

	template <template <class O> class Tester, typename FirstType, typename... Types, typename F>
	constexpr auto run_if(F&& f, type_list<FirstType, Types...>*) {
		if constexpr (Tester<FirstType>::value) {
			return f.template operator()<FirstType>();
		} else {
			return run_if<Tester, Types...>(f, static_cast<type_list<Types...>*>(nullptr));
		}
	}

	template <typename... Types, typename F>
	constexpr std::size_t count_if(F&& f, type_list<Types...>*) {
		return (static_cast<std::size_t>(f.template operator()<Types>()) + ...);
	}

	
	template <typename TL, template <class O> class Transformer>
	struct transform_type {
		template <typename... Types>
		constexpr static type_list<Transformer<Types>...>* helper(type_list<Types...>*);

		using type = std::remove_pointer_t<decltype(helper(static_cast<TL*>(nullptr)))>;
	};
	
	template <typename TL, template <class... O> class Transformer>
	struct transform_type_all {
		template <typename... Types>
		constexpr static Transformer<Types...>* helper(type_list<Types...>*);

		using type = std::remove_pointer_t<decltype(helper(static_cast<TL*>(nullptr)))>;
	};

	template <typename TL, typename T>
	struct add_type {
		template <typename... Types>
		constexpr static type_list<Types..., T>* helper(type_list<Types...>*);

		using type = std::remove_pointer_t<decltype(helper(static_cast<TL*>(nullptr)))>;
	};
	
	template <typename TL, template <class O> class Predicate>
	struct split_types_if {
		template <typename ListTrue, typename ListFalse, typename Front, typename... Rest >
		constexpr static auto helper(type_list<Front, Rest...>*) {
			if constexpr (Predicate<Front>::value) {
				using NewListTrue = typename add_type<ListTrue, Front>::type;

				if constexpr (sizeof...(Rest) > 0) {
					return helper<NewListTrue, ListFalse>(static_cast<type_list<Rest...>*>(nullptr));
				} else {
					return static_cast<type_pair<NewListTrue, ListFalse>*>(nullptr);
				}
			} else {
				using NewListFalse = typename add_type<ListFalse, Front>::type;

				if constexpr (sizeof...(Rest) > 0) {
					return helper<ListTrue, NewListFalse>(static_cast<type_list<Rest...>*>(nullptr));
				} else {
					return static_cast<type_pair<ListTrue, NewListFalse>*>(nullptr);
				}
			}
		}

		using list_pair = 
			std::remove_pointer_t<decltype(helper<type_list<>, type_list<>>(static_cast<TL*>(nullptr)))>;
	};

	template<typename First, typename... Types>
	constexpr bool is_unique_types(type_list<First, Types...>*) {
		if constexpr ((std::is_same_v<First, Types> || ...))
			return false;
		else {
			if constexpr (sizeof...(Types) == 0)
				return true;
			else
				return is_unique_types<Types...>({});
		}
	}

	template <typename T, typename... Types>
	static constexpr bool contains_type(type_list<Types...>*) {
		return (std::is_same_v<T, Types> || ...);
	}

	struct merger {
		template <typename... Left>
		static auto helper(type_list<Left...>*, type_list<>*)
		-> type_list<Left...>*;

	#ifdef _MSC_VER
		template <typename... Left, typename FirstRight, typename... Right>
		static auto helper(type_list<Left...>*, type_list<FirstRight, Right...>*)
		-> decltype(helper(
			static_cast<type_list<Left...>*>(nullptr),
			static_cast<type_list<Right...>*>(nullptr)));

		template <typename... Left, typename FirstRight, typename... Right>
		static auto helper(type_list<Left...>*, type_list<FirstRight, Right...>*)
		-> decltype(helper(
			static_cast<type_list<Left..., FirstRight>*>(nullptr),
			static_cast<type_list<Right...>*>(nullptr)))
		requires(!contains_type<FirstRight>(static_cast<type_list<Left...>*>(nullptr)));
	#else
		template <typename... Types1, typename First2, typename... Types2>
		constexpr static auto* helper(type_list<Types1...>*, type_list<First2, Types2...>*) {
			using NewTL2 = type_list<Types2...>;

			if constexpr (contains_type<First2>(static_cast<type_list<Types1...>*>(nullptr))) {
				if constexpr(sizeof...(Types2) == 0)
					return static_cast<type_list<Types1...>*>(nullptr);
				else
					return helper(static_cast<type_list<Types1...>*>(nullptr), static_cast<NewTL2*>(nullptr));
			} else {
				if constexpr(sizeof...(Types2) == 0)
					return static_cast<type_list<Types1..., First2>*>(nullptr);
				else
					return helper(static_cast<type_list<Types1..., First2>*>(nullptr), static_cast<NewTL2*>(nullptr));
			}
		}
	#endif
	};

} // namespace impl

template <impl::TypeList TL>
constexpr size_t type_list_size = impl::type_list_size<TL>::value;

// Classes can inherit from type_list_indices with a provided type_list
// to have 'index_of(T*)' functions injected into it, for O(1) lookups
// of the indices of the types in the type_list
template<typename TL>
using type_list_indices = decltype(impl::type_list_indices(static_cast<TL*>(nullptr)));


// Transforms the types in a type_list
// Takes transformer that results in new type, like remove_cvref_t
template <impl::TypeList TL, template <class O> class Transformer>
using transform_type = typename impl::transform_type<TL, Transformer>::type;

// Transforms all the types in a type_list at once
// Takes a transformer that results in a new type
// Ex.
// 	template <typename... Types>
//  using transformer = std::tuple<entity_id, Types...>;
template <impl::TypeList TL, template <class... O> class Transformer>
using transform_type_all = typename impl::transform_type_all<TL, Transformer>::type;

template <impl::TypeList TL, template <class O> class Predicate>
using split_types_if = typename impl::split_types_if<TL, Predicate>::list_pair;


// Applies the functor F to each type in the type list.
// Takes lambdas of the form '[]<typename T>() {}'
template <impl::TypeList TL, typename F>
constexpr void for_each_type(F&& f) {
	impl::for_each_type(f, static_cast<TL*>(nullptr));
}

// Applies the functor F to a specific type in the type list.
// Takes lambdas of the form '[]() {}'
template <typename T, impl::TypeList TL, typename F>
constexpr void for_specific_type(F&& f) {
	impl::for_specific_type<T>(f, static_cast<TL*>(nullptr));
}

// Applies the functor F when a specific type in the type list is found,
// applies NF when not found
// Takes lambdas of the form '[]() {}'
template <typename T, impl::TypeList TL, typename F, typename NF>
constexpr void for_specific_type_or(F&& f, NF&& nf) {
	impl::for_specific_type_or<T>(f, nf, static_cast<TL*>(nullptr));
}

// Applies the functor F to all types in the type list.
// Takes lambdas of the form '[]<typename ...T>() {}'
template <impl::TypeList TL, typename F>
constexpr decltype(auto) apply_type(F&& f) {
	return impl::apply_type(f, static_cast<TL*>(nullptr));
}

// Applies the bool-returning functor F to each type in the type list.
// Returns true if all of them return true.
// Takes lambdas of the form '[]<typename T>() -> bool {}'
template <impl::TypeList TL, typename F>
constexpr bool all_of_type(F&& f) {
	return impl::all_of_type(f, static_cast<TL*>(nullptr));
}

// Applies the bool-returning functor F to each type in the type list.
// Returns true if any of them return true.
// Takes lambdas of the form '[]<typename T>() -> bool {}'
template <impl::TypeList TL, typename F>
constexpr bool any_of_type(F&& f) {
	return impl::any_of_type(f, static_cast<TL*>(nullptr));
}

// Runs F once when a type satifies the tester. F takes a type template parameter and can return a value.
template <template <class O> class Tester, impl::TypeList TL, typename F>
constexpr auto run_if(F&& f) {
	return impl::run_if<Tester>(f, static_cast<TL*>(nullptr));
}

// Returns the count of all types that satisfy the predicate. F takes a type template parameter and returns a boolean.
template <impl::TypeList TL, typename F>
constexpr std::size_t count_if(F&& f) {
	return impl::count_if(f, static_cast<TL*>(nullptr));
}

// Returns true if all types in the list are unique
template <impl::TypeList TL>
constexpr bool is_unique_types() {
	return impl::is_unique_types(static_cast<TL*>(nullptr));
}

// Returns true if a type list contains the type
template <typename T, impl::TypeList TL>
constexpr bool contains_type() {
	return impl::contains_type<T>(static_cast<TL*>(nullptr));
}

// concatenates two type_list
template <impl::TypeList TL1, impl::TypeList TL2>
using concat_type_lists = std::remove_pointer_t<decltype(
	[] {
		auto constexpr meh = 
			[]<typename... Types1, typename... Types2>(type_list<Types1...>*, type_list<Types2...>*)
			-> type_list<Types1..., Types2...>* {
				return nullptr;
			};

		return meh(static_cast<TL1*>(nullptr), static_cast<TL2*>(nullptr));
	}())>;

// merge two type_list, duplicate types are ignored
template <typename TL1, typename TL2>
using merge_type_lists = std::remove_pointer_t<decltype(
	impl::merger::helper(static_cast<TL1*>(nullptr), static_cast<TL2*>(nullptr)))>;

} // namespace ecs::detail
#endif // !TYPE_LIST_H_

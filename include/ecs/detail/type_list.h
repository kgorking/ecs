#ifndef TYPE_LIST_H_
#define TYPE_LIST_H_

namespace ecs::detail {

//
// Helper templates for working with types at compile-time

// inspired by https://devblogs.microsoft.com/cppblog/build-throughput-series-more-efficient-template-metaprogramming/

template <typename...>
struct type_list;

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
	// type_list_at.
	template <int, typename>
	struct type_list_at;
	template <int I, typename Type, typename... Types>
	struct type_list_at<I, type_list<Type, Types...>> {
		using type = typename type_list_at<I - 1, type_list<Types...>>::type;
	};
	template <typename Type, typename... Types>
	struct type_list_at<0, type_list<Type, Types...>> {
		using type = Type;
	};

	//
	// type_list_at_or.
	template <int, typename OrType, typename TypeList>
	struct type_list_at_or;

	template <int I, typename OrType, typename Type, typename... Types>
	struct type_list_at_or<I, OrType, type_list<Type, Types...>> {
		using type = typename type_list_at_or<I - 1, OrType, type_list<Types...>>::type;
	};

	template <typename Type, typename OrType, typename... Types>
	struct type_list_at_or<0, OrType, type_list<Type, Types...>> {
		using type = Type;
	};

	template <typename Type, typename OrType, typename... Types>
	struct type_list_at_or<int{-1}, OrType, type_list<Type, Types...>> {
		using type = OrType;
	};


	//
	// type_list concepts
	template <class TL>
	concept TypeList = detect_type_list(static_cast<TL*>(nullptr));



	//
	// helper functions
	template <typename... Types, typename F>
	constexpr void for_each_type(F&& f, type_list<Types...>*) {
		(f.template operator()<Types>(), ...);
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
} // namespace impl

template <impl::TypeList TL>
constexpr size_t type_list_size = impl::type_list_size<TL>::value;

template <int I, impl::TypeList TL>
using type_list_at = typename impl::type_list_at<I, TL>::type;

template <int I, impl::TypeList TL, typename OrType>
using type_list_at_or = typename impl::type_list_at_or<I, OrType, TL>::type;

// Applies the functor F to each type in the type list.
// Takes lambdas of the form '[]<typename T>() {}'
template <impl::TypeList TL, typename F>
constexpr void for_each_type(F&& f) {
	impl::for_each_type(f, static_cast<TL*>(nullptr));
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

} // namespace ecs::detail
#endif // !TYPE_LIST_H_

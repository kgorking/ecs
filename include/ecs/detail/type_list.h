#ifndef TYPE_LIST_H_
#define TYPE_LIST_H_

namespace ecs::detail {

//
// Helper templates for working with types at compile-time

// inspired by https://devblogs.microsoft.com/cppblog/build-throughput-series-more-efficient-template-metaprogramming/

template <typename...>
struct type_list;

namespace impl {
	// Implementation of type_list_size.
	template <typename>
	struct type_list_size;
	template <typename... Types>
	struct type_list_size<type_list<Types...>> {
		static constexpr size_t value = sizeof...(Types);
	};

	// Implementation of type_list_at.
	template <size_t, typename>
	struct type_list_at;
	template <size_t I, typename Type, typename... Types>
	struct type_list_at<I, type_list<Type, Types...>> {
		using type = typename type_list_at<I - 1, type_list<Types...>>::type;
	};
	template <typename Type, typename... Types>
	struct type_list_at<0, type_list<Type, Types...>> {
		using type = Type;
	};

	template <typename Type, typename F>
	constexpr decltype(auto) invoke_type(F &&f) {
		return f(static_cast<Type*>(nullptr));
	}

	template <typename... Types, typename F>
	constexpr void for_each_type(F &&f, type_list<Types...>*) {
		(invoke_type<Types>(f), ...);
	}

	template <typename... Types, typename F>
	constexpr decltype(auto) apply_type(F &&f, type_list<Types...>*) {
		return f(static_cast<Types*>(nullptr)...);
	}

	template <typename... Types, typename F>
	constexpr bool all_of_type(F &&f, type_list<Types...>*) {
		return (invoke_type<Types>(f) && ...);
	}

	template <typename... Types, typename F>
	constexpr bool any_of_type(F &&f, type_list<Types...>*) {
		return (invoke_type<Types>(f) || ...);
	}
} // namespace impl

template <typename Types>
constexpr size_t type_list_size = impl::type_list_size<Types>::value;

template <size_t I, typename Types>
using type_list_at = typename impl::type_list_at<I, Types>::type;


template <typename TypeList, typename F>
constexpr void for_each_type(F &&f) {
	impl::for_each_type(f, static_cast<std::add_pointer_t<TypeList>>(nullptr));
}

template <typename TypeList, typename F>
constexpr decltype(auto) apply_type(F &&f) {
	return impl::apply_type(f, static_cast<TypeList*>(nullptr));
}

template <typename TypeList, typename F>
constexpr bool all_of_type(F &&f) {
	return impl::all_of_type(f, static_cast<TypeList*>(nullptr));
}

template <typename TypeList, typename F>
constexpr bool any_of_type(F &&f) {
	return impl::any_of_type(f, static_cast<TypeList*>(nullptr));
}

#define M_for_each_type(typelist, code_for_T_in_braces) for_each_type<typelist>([&]<typename T>(T*) { code_for_T_in_braces } )
#define M_apply_type(typelist, code_for_Tpack_in_braces) apply_type<typelist>([&]<typename ...T>(T*...) { code_for_Tpack_in_braces } )
#define M_all_of_type(typelist, code_for_T_in_braces) all_of_type<typelist>([&]<typename T>(T*) { code_for_T_in_braces } )
#define M_any_of_type(typelist, code_for_T_in_braces) any_of_type<typelist>([&]<typename T>(T*) { code_for_T_in_braces } )

} // namespace ecs::detail
#endif // !TYPE_LIST_H_

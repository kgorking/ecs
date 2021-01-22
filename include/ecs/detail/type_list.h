#ifndef TYPE_LIST_H_
#define TYPE_LIST_H_

namespace ecs::detail {

//
// Helper templates for working with types at compile-time

// https://devblogs.microsoft.com/cppblog/build-throughput-series-more-efficient-template-metaprogramming/

template <typename...>
struct type_list;

namespace impl {
	// Implementation of type_list_size.
	template <typename>
	struct type_list_size_impl;
	template <typename... Types>
	struct type_list_size_impl<type_list<Types...>> {
		static constexpr size_t value = sizeof...(Types);
	};

	// Implementation of type_list_at.
	template <size_t, typename>
	struct type_list_at_impl;
	template <size_t I, typename Type, typename... Types>
	struct type_list_at_impl<I, type_list<Type, Types...>> {
		using type = typename type_list_at_impl<I - 1, type_list<Types...>>::type;
	};
	template <typename Type, typename... Types>
	struct type_list_at_impl<0, type_list<Type, Types...>> {
		using type = Type;
	};

	template <typename... Types, typename F>
	void for_each_type_impl (F &&f, type_list<Types...>*) {
		(f(static_cast<Types*>(nullptr)), ...);
	}
} // namespace impl

template <typename Types>
constexpr size_t type_list_size = impl::type_list_size_impl<Types>::value;

template <size_t I, typename Types>
using type_list_at = typename impl::type_list_at_impl<I, Types>::type;

template <typename TypeList, typename F>
void for_each_type(F &&f) {
	impl::for_each_type_impl(f, static_cast<std::add_pointer_t<TypeList>>(nullptr));
}

} // namespace ecs::detail
#endif // !TYPE_LIST_H_

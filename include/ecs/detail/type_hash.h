#ifndef ECS_TYPE_HASH
#define ECS_TYPE_HASH

#include <cstdint>

// Beware of using this with local defined structs/classes
// https://developercommunity.visualstudio.com/content/problem/1010773/-funcsig-missing-data-from-locally-defined-structs.html

namespace ecs::detail {

using type_hash = std::uint64_t;

template <typename T>
consteval type_hash get_type_hash() {
	type_hash const prime = 0x100000001b3;
#ifdef _MSC_VER
	char const* string = __FUNCDNAME__; // has full type info, but is not very readable
#else
	char const* string = __PRETTY_FUNCTION__;
#endif
	type_hash hash = 0xcbf29ce484222325;
	while (*string != '\0') {
		hash ^= static_cast<type_hash>(*string);
		hash *= prime;
		string += 1;
	}

	return hash;
}

template <typename TypesList>
consteval auto get_type_hashes_array() {
	return for_all_types<TypesList>([]<typename... Types>() {
		return std::array<detail::type_hash, sizeof...(Types)>{get_type_hash<Types>()...};
	});
}

} // namespace ecs::detail

#endif // !ECS_TYPE_HASH

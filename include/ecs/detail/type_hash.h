#ifndef ECS_TYPE_HASH
#define ECS_TYPE_HASH

#include <cstdint>
#include <string_view>

// Beware of using this with local defined structs/classes
// https://developercommunity.visualstudio.com/content/problem/1010773/-funcsig-missing-data-from-locally-defined-structs.html

namespace ecs::detail {

using type_hash = std::uint64_t;

template <typename T>
consteval auto get_type_name() {
#ifdef _MSC_VER
	std::string_view fn = __FUNCSIG__;
	auto const type_start = fn.find("get_type_name<") + 14;
	auto const type_end = fn.rfind(">(void)");
	return fn.substr(type_start, type_end - type_start);
#else
	std::string_view fn = __PRETTY_FUNCTION__;
	auto const type_start = fn.rfind("T = ") + 4;
	auto const type_end = fn.rfind("]");
	return fn.substr(type_start, type_end - type_start);
#endif
}

template <typename T>
consteval type_hash get_type_hash() {
	type_hash const prime = 0x100000001b3;
#ifdef _MSC_VER
	std::string_view string = __FUNCDNAME__; // has full type info, but is not very readable
#else
	std::string_view string = __PRETTY_FUNCTION__;
#endif
	type_hash hash = 0xcbf29ce484222325;
	for (char const value : string) {
		hash ^= static_cast<type_hash>(value);
		hash *= prime;
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

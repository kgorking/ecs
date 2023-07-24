#ifndef ECS_DETAIL_FLAGS_H
#define ECS_DETAIL_FLAGS_H

#include "../flags.h"
#include <type_traits>

// Some helpers concepts to detect flags
namespace ecs::detail {

template <typename T>
using flags = typename std::remove_cvref_t<T>::_ecs_flags;

template <typename T>
concept tagged = std::is_base_of_v<ecs::flag::tag, flags<T>>;

template <typename T>
concept transient = std::is_base_of_v<ecs::flag::transient, flags<T>>;

template <typename T>
concept immutable = std::is_base_of_v<ecs::flag::immutable, flags<T>>;

template <typename T>
concept global = std::is_base_of_v<ecs::flag::global, flags<T>>;

template <typename T>
concept local = !global<T>;

template <typename T>
concept persistent = !transient<T>;

template <typename T>
concept unbound = (tagged<T> || global<T>); // component is not bound to a specific entity (ie static)


//
template <typename T>
struct is_tagged : std::bool_constant<tagged<T>> {};

template <typename T>
struct is_transient : std::bool_constant<transient<T>> {};

template <typename T>
struct is_immutable : std::bool_constant<immutable<T>> {};

template <typename T>
struct is_global : std::bool_constant<global<T>> {};

template <typename T>
struct is_local : std::bool_constant<!global<T>> {};

template <typename T>
struct is_persistent : std::bool_constant<!transient<T>> {};

template <typename T>
struct is_unbound : std::bool_constant<tagged<T> || global<T>> {};

} // namespace ecs::detail

#endif // !ECS_DETAIL_COMPONENT_FLAGS_H

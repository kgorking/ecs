#ifndef __DETAIL_FLAGS_H
#define __DETAIL_FLAGS_H

#include <type_traits>
#include "../flags.h"

// Some helpers concepts to detect flags
namespace ecs::detail {
    template<typename T>
    using flags = typename std::remove_cvref_t<T>::_ecs_flags;

    template<typename T>
    concept tagged = std::is_base_of_v<ecs::flag::tag, flags<T>>;

    template<typename T>
    concept shared = std::is_base_of_v<ecs::flag::share, flags<T>>;

    template<typename T>
    concept transient = std::is_base_of_v<ecs::flag::transient, flags<T>>;

    template<typename T>
    concept immutable = std::is_base_of_v<ecs::flag::immutable, flags<T>>;

    template<typename T>
    concept global = std::is_base_of_v<ecs::flag::global, flags<T>>;

    template<typename T>
    concept local = !global<T> && !shared<T>;

    template<typename T>
    concept persistent = !transient<T>;

    template<typename T>
    concept unbound = (shared<T> || tagged<T> || global<T>); // component is not bound to a specific entity (ie static)
} // namespace ecs::detail

#endif // !__DETAIL_COMPONENT_FLAGS_H

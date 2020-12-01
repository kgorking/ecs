#ifndef __PARENT_H_
#define __PARENT_H_

#include "entity_id.h"
#include "detail/system_defs.h"
#include <tuple>

namespace ecs::detail {
    template<typename Component, typename Pools>
    auto get_component(entity_id const, Pools const&);
}

namespace ecs {

    // Special component that allows parent/child relationships
    template<typename ... ParentTypes>
    struct parent : entity_id {

        explicit parent(entity_id id)
            : entity_id(id) {
        }

        parent(parent const&) = default;
        parent& operator=(parent const&) = default;

        entity_id id() const {
            return (detail::entity_type)*this;
        }

        template<typename T>
        T& get() {
            static_assert((std::is_same_v<T, ParentTypes> || ...), "T is not specified in the parent component");
            return *std::get<T*>(parent_components);
        }

        template<typename T>
        T const& get() const {
            static_assert((std::is_same_v<T, ParentTypes> || ...), "T is not specified in the parent component");
            return *std::get<T*>(parent_components);
        }

        // used internally by detectors
        struct _ecs_parent {};

    private:
        using pool_tuple = std::tuple<detail::pool<ParentTypes>...>;
        using self = parent<ParentTypes...>;

        template<typename Component, typename Pools>
        friend auto detail::get_component(entity_id const, Pools const&);

        parent(entity_id id, std::tuple<ParentTypes*...> tup)
            : entity_id(id)
            , parent_components(tup) {
        }

        std::tuple<ParentTypes*...> parent_components;
    };
} // namespace ecs

#endif // !__PARENT_H_

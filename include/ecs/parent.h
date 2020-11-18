#ifndef __PARENT_H_
#define __PARENT_H_

#include "entity_id.h"
#include <tuple>

namespace ecs {
    // Special component that allows parent/child relationships
    template<typename ... ParentTypes>
    struct parent : entity_id {

        explicit parent(entity_id id)
            : entity_id(id) {
        }

        // used internally by detectors
        struct _ecs_parent {};
    private:
        std::tuple<ParentTypes*...> parent_components;
    };
} // namespace ecs

#endif // !__PARENT_H_

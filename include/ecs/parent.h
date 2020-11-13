#ifndef __PARENT_H_
#define __PARENT_H_

#include "entity_id.h"
//#include "detail/system_base.h"

namespace ecs {
    namespace detail {
        class system_base;
    }

    // Special component that allows parent/child relationships
    template<typename ... ParentTypes>
    struct parent : entity_id {

        explicit parent(entity_id id)
            : entity_id(id) {
        }

        struct _ecs_parent {};
    private:
        void* test = nullptr;

        friend class detail::system_base;
    };
} // namespace ecs

#endif // !__PARENT_H_

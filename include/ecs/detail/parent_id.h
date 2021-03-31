#ifndef ECS_DETAIL_PARENT_H
#define ECS_DETAIL_PARENT_H

#include "../entity_id.h"

namespace ecs::detail {
    // The parent type stored internally in component pools
    struct parent_id : entity_id {};
}
// namespace ecs::detail

#endif // !ECS_DETAIL_PARENT_H

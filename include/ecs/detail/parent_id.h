#ifndef __DETAIL_PARENT_H
#define __DETAIL_PARENT_H

#include "../entity_id.h"

namespace ecs::detail {
    // The parent type stored internally in component pools
    struct parent_id : entity_id {};
}
// namespace ecs::detail

#endif // !__DETAIL_PARENT_H

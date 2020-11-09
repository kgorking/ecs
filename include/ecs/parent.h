#ifndef __PARENT_H_
#define __PARENT_H_

#include "entity_id.h"

namespace ecs {
    // Special component that allows parent/child relationships
    struct parent : entity_id {
    };
}

#endif // !__PARENT_H_

#ifndef ECS_DETAIL_PARENT_H
#define ECS_DETAIL_PARENT_H

#include "../entity_id.h"

namespace ecs::detail {

// The parent type stored internally in component pools
struct parent_id : entity_id {
	constexpr parent_id(detail::entity_type _id) noexcept : entity_id(_id) {}
};

} // namespace ecs::detail

#endif // !ECS_DETAIL_PARENT_H

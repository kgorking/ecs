#ifndef ECS_DETAIL_LISTARRAY_H
#define ECS_DETAIL_LISTARRAY_H

#include <vector>
#include <memory_resource>

namespace ecs::detail {
	template <typename T, typename Alloc = std::allocator<T>>
	struct listarray {
		struct node {
			int vec_offset = -1;
			int next = -1;
		};

		std::vector<node> nodes;
		std::vector<T> vec;
	};
	static_assert(sizeof(listarray<int>) == 64);

} // namespace ecs::detail

#endif // !ECS_DETAIL_LISTARRAY_H

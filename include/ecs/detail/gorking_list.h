#ifndef ECS_DETAIL_GORKING_LIST_H
#define ECS_DETAIL_GORKING_LIST_H

namespace ecs::detail {
	template<typename T>
	struct list {
		struct node {
			node* middle;
			node* next;
			T data;
		};

		node* head:
	};
}

#endif // !ECS_DETAIL_GORKING_LIST_H

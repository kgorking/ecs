#ifndef ECS_DETAIL_OPERATION_H
#define ECS_DETAIL_OPERATION_H

#include "contract.h"
#include "../entity_id.h"

namespace ecs::detail {
	struct operation final {
		template <typename Arguments, typename Fn>
		explicit operation(Arguments* unused_args, Fn& fn) : arguments{nullptr}, function(&fn) {
			Pre(unused_args == nullptr, "This value is only used to get the type; pass nullptr.");

			op = [](entity_id id, entity_offset offset, void* p1, void* p2) {
				auto* arg = static_cast<Arguments*>(p1);
				auto* func = static_cast<Fn*>(p2);
				(*arg)(*func, id, offset);
			};
		}

		void set_args(void* args) {
			arguments = args;
		}

		void run(entity_id id, entity_offset offset) const {
			op(id, offset, arguments, function);
		}

	private:
		void* arguments;
		void* function;
		void (*op)(entity_id id, entity_offset offset, void*, void*);
	};
} // namespace ecs::detail

#endif

#ifndef ECS_PARENT_H_
#define ECS_PARENT_H_

#include "detail/system_defs.h"
#include "entity_id.h"

// forward decls
namespace ecs::detail {
	template <typename Pools> struct pool_entity_walker;
	template <typename Pools> struct pool_range_walker;

	template<int Size>
	struct void_ptr_storage {
		void* te_ptrs[Size];
	};
	struct empty_storage {
	};
}

namespace ecs {
// Special component that allows parent/child relationships
template <typename... ParentTypes>
struct parent : entity_id,
				private std::conditional_t<(sizeof...(ParentTypes) > 0), detail::void_ptr_storage<sizeof...(ParentTypes)>, detail::empty_storage> {

	explicit parent(entity_id id) : entity_id(id) {}

	parent(parent const&) = default;
	parent& operator=(parent const&) = default;

	entity_id id() const {
		return static_cast<entity_id>(*this);
	}

	template <typename T>
	[[nodiscard]] T& get() const {
		static_assert((std::is_same_v<T, ParentTypes> || ...), "T is not specified in the parent component");
		return *static_cast<T*>(this->te_ptrs[detail::index_of<T, indexer>()]);
	}

	// used internally by detectors
	struct _ecs_parent {};

private:
	using indexer = detail::impl::type_list_index<0, ParentTypes...>;

	template <typename Component>
	friend decltype(auto) detail::extract_arg_lambda(auto& cmp, ptrdiff_t offset, auto pools);

	template <typename Pools> friend struct detail::pool_entity_walker;
	template <typename Pools> friend struct detail::pool_range_walker;

	parent(entity_id id, ParentTypes*... pt)
		requires(sizeof...(ParentTypes) > 0)
		: entity_id(id) {
		((this->te_ptrs[detail::index_of<ParentTypes, indexer>()] = static_cast<void*>(pt)), ...);
	}
};
} // namespace ecs

#endif // !ECS_PARENT_H_

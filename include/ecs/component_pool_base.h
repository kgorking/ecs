#ifndef __COMPONENT_POOL_BASE
#define __COMPONENT_POOL_BASE

namespace ecs::detail {
	// The baseclass of typed component pools
	// TODO try and get rid of this baseclass
	class component_pool_base {
	public:
		component_pool_base() = default;
		component_pool_base(component_pool_base const&) = delete;
		component_pool_base(component_pool_base&&) = delete;
		component_pool_base& operator = (component_pool_base const&) = delete;
		component_pool_base& operator = (component_pool_base&&) = delete;
		virtual ~component_pool_base() = default;

		virtual void process_changes() = 0;
		virtual void clear_flags() = 0;
		virtual void clear() = 0;
	};
}

#endif // !__COMPONENT_POOL_BASE

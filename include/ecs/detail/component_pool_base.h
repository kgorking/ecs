#ifndef ECS_COMPONENT_POOL_BASE
#define ECS_COMPONENT_POOL_BASE

namespace ecs::detail {
// The baseclass of typed component pools
class component_pool_base {
public:
	component_pool_base() = default;
	component_pool_base(component_pool_base const&) = delete;
	component_pool_base(component_pool_base&&) = delete;
	component_pool_base& operator=(component_pool_base const&) = delete;
	component_pool_base& operator=(component_pool_base&&) = delete;
	virtual ~component_pool_base() = default;

	virtual void process_changes() = 0;
	virtual void clear_flags() = 0;
	virtual void clear() = 0;

#ifdef ECS_ENABLE_CONTRACTS_AUDIT
	// facilitate variant implementation.
	// Called from other component pools.
	virtual void remove_variant(entity_range const range) = 0;
#endif
};
} // namespace ecs::detail

#endif // !ECS_COMPONENT_POOL_BASE

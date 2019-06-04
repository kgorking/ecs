#pragma once
#include <gsl/gsl>
#include "types.h"

namespace ecs::detail
{
	// The baseclass of typed component pools
	class component_pool_base
	{
	public:
		component_pool_base() = default;
		component_pool_base(component_pool_base const&) = delete;
		component_pool_base(component_pool_base&&) = default;
		component_pool_base& operator =(component_pool_base const&) = delete;
		component_pool_base& operator =(component_pool_base&&) = default;
		virtual ~component_pool_base() = default;

		virtual void process_changes() = 0;
		virtual bool was_changed() const noexcept  = 0;
		virtual void clear() = 0;
		virtual void clear_flags() noexcept = 0;

		virtual gsl::span<entity_id const> get_entities() const noexcept = 0;

		//virtual void remove(entity_id id) = 0;
	};
}

#pragma once

namespace ecs::detail
{
	// The baseclass of typed component pools
	// TODO try and get rid of this baseclass
	class component_pool_base
	{
	public:
		virtual ~component_pool_base() = default;

		virtual void process_changes() = 0;
		virtual void clear_flags() noexcept = 0;
		virtual void clear() noexcept = 0;
	};
}

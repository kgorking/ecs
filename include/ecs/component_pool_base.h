#pragma once

namespace ecs::detail
{
	// The baseclass of typed component pools
	class component_pool_base
	{
	public:
		virtual void process_changes() = 0;
		virtual void clear_flags() noexcept = 0;
		virtual void clear() = 0;
	};
}

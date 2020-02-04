#pragma once

namespace ecs::detail {
	class context;
}

namespace ecs
{
	class system
	{
		friend class detail::context;

		// Process changes to component layouts
		virtual void process_changes() = 0;

	public:
		// Run this system on all of its associated components
		virtual void update() = 0;

	public:
		system() = default;
		virtual ~system() = default;
		system(system const&) = delete;
		system(system&&) = default;
		system& operator =(system const&) = delete;
		system& operator =(system&&) = default;
	};
}

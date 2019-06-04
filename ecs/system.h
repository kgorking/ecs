#pragma once

namespace ecs
{
	class system
	{
	public:
		// Run this system on all of its associated components
		virtual void update() noexcept = 0;

		// Process changes to component layouts
		virtual void process_changes() = 0;

	public:
		system() = default;
		system(system const&) = delete;
		system(system&&) = default;
		virtual ~system() = default;
		system& operator =(system const&) = delete;
		system& operator =(system&&) = default;
	};
}

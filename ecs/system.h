#pragma once

namespace ecs
{
	//extern void commit_changes();

	class system
	{
		friend void commit_changes();

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

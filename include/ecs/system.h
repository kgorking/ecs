#pragma once

namespace ecs::detail {
	class context;
}

namespace ecs
{
	class system
	{
	public:
		system() = default;
		virtual ~system() = default;
		system(system const&) = delete;
		system(system&&) = default;
		system& operator =(system const&) = delete;
		system& operator =(system&&) = default;

		// Run this system on all of its associated components
		virtual void update() = 0;

		// Enables this system for updates and runs
		void enable() { enabled = true; }

		// Prevent this system from being updated or run
		void disable() { enabled = false; }

		// Sets wheter the system is enabled or disabled
		void set_enable(bool enabled) { this->enabled = enabled; }

		// Returns true if this system is enabled
		bool is_enabled() const { return enabled; }

	private:
		friend class detail::context;

		// Process changes to component layouts
		virtual void process_changes() = 0;


		// Whether this system is enabled or disabled. Disabled systems are neither updated nor run.
		bool enabled = true;
	};
}

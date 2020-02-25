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
		void enable() { set_enable(true); }

		// Prevent this system from being updated or run
		void disable() { set_enable(false); }

		// Sets wheter the system is enabled or disabled
		void set_enable(bool is_enabled) {
			enabled = is_enabled;
			if (is_enabled) {
				process_changes(true);
			}
		}

		// Returns true if this system is enabled
		bool is_enabled() const { return enabled; }

		// Returns the group this system belongs to
		virtual int get_group() const noexcept = 0;

	private:
		friend class detail::context;

		// Process changes to component layouts
		virtual void process_changes(bool force_rebuild = false) = 0;


		// Whether this system is enabled or disabled. Disabled systems are neither updated nor run.
		bool enabled = true;
	};
}

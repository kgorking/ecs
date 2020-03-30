#ifndef __SYSTEM
#define __SYSTEM

namespace ecs::detail {
	class context;
}

namespace ecs {
	class system_base {
	public:
		system_base() = default;
		virtual ~system_base() = default;
		system_base(system_base const&) = delete;
		system_base(system_base&&) = default;
		system_base& operator =(system_base const&) = delete;
		system_base& operator =(system_base&&) = default;

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
		[[nodiscard]] bool is_enabled() const { return enabled; }

		// Returns the group this system belongs to
		[[nodiscard]] virtual int get_group() const noexcept = 0;

	private:
		// Only allow the context class to call 'process_changes'
		friend class detail::context;

		// Process changes to component layouts
		virtual void process_changes(bool force_rebuild = false) = 0;


		// Whether this system is enabled or disabled. Disabled systems are neither updated nor run.
		bool enabled = true;
	};
}

#endif // !__SYSTEM

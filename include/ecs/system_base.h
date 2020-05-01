#ifndef __SYSTEM
#define __SYSTEM

#include "type_hash.h"

namespace ecs::detail {
	class context;
	class system_scheduler;
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
		[[nodiscard]]
		bool is_enabled() const { return enabled; }

		// Returns the group this system belongs to
		[[nodiscard]]
		virtual int get_group() const noexcept = 0;

		// Get the signature of the system
		[[nodiscard]]
		virtual std::string get_signature() const noexcept = 0;

		// Returns true if this system has a dependency on another system
		[[nodiscard]]
		virtual bool has_type(detail::type_hash hash) const noexcept = 0;

		// Returns true if this system has a dependency on another system
		[[nodiscard]]
		virtual bool depends_on(system_base const&) const noexcept = 0;

		// Returns true if this system writes data to any component
		[[nodiscard]]
		virtual bool writes_to_any_components() const noexcept = 0;

		// Returns true if this system writes data to a specific component
		[[nodiscard]]
		virtual bool writes_to_component(detail::type_hash hash) const noexcept = 0;

		// Returns the hashes of components the system writes to
		//virtual std::vector<detail::type_hash> get_write_component_hashes() const noexcept = 0;

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

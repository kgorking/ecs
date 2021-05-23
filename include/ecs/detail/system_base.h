#ifndef ECS_SYSTEM_BASE
#define ECS_SYSTEM_BASE

#include "type_hash.h"
#include <span>

namespace ecs::detail {
class context;

class system_base {
public:
	system_base() = default;
	virtual ~system_base() = default;
	system_base(system_base const&) = delete;
	system_base(system_base&&) = default;
	system_base& operator=(system_base const&) = delete;
	system_base& operator=(system_base&&) = default;

	// Run this system on all of its associated components
	virtual void run() = 0;

	// Enables this system for updates and runs
	void enable() {
		set_enable(true);
	}

	// Prevent this system from being updated or run
	void disable() {
		set_enable(false);
	}

	// Sets wheter the system is enabled or disabled
	void set_enable(bool is_enabled) {
		enabled = is_enabled;
		if (is_enabled) {
			process_changes(true);
		}
	}

	// Returns true if this system is enabled
	[[nodiscard]] bool is_enabled() const {
		return enabled;
	}

	void add_predecessor(system_base* sys) {
		predecessors.push_back(sys);
	}

	void add_sucessor(system_base* sys) {
		sucessors.push_back(sys);
	}

	std::span<system_base *const> get_predecessors() const {
		return {predecessors.begin(), predecessors.size()};
	}

	// Returns the group this system belongs to
	[[nodiscard]] virtual int get_group() const noexcept = 0;

	// Get the hashes of types used by the system with const/reference qualifiers removed
	[[nodiscard]] virtual std::span<detail::type_hash const> get_type_hashes() const noexcept = 0;

	// Returns true if this system uses the type
	[[nodiscard]] virtual bool has_component(detail::type_hash hash) const noexcept = 0;

	// Returns true if this system has a dependency on another system
	[[nodiscard]] virtual bool depends_on(system_base const*) const noexcept = 0;

	// Returns true if this system writes data to a specific component
	[[nodiscard]] virtual bool writes_to_component(detail::type_hash hash) const noexcept = 0;

private:
	// Only allow the context class to call 'process_changes'
	friend class detail::context;

	// Process changes to component layouts
	virtual void process_changes(bool force_rebuild = false) = 0;

	// Whether this system is enabled or disabled. Disabled systems are neither updated nor run.
	bool enabled = true;

	std::vector<system_base*> predecessors;
	std::vector<system_base*> sucessors;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_BASE

#ifndef ECS_SYSTEM_BASE
#define ECS_SYSTEM_BASE

#include "type_hash.h"
#include "contract.h"
#include "operation.h"
#include <span>
#include <unordered_set>

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

	// Run this system on all of its associated entities
	virtual void run() = 0;

	// Enables this system for updates and runs
	void enable() {
		set_enable(true);
	}

	// Prevent this system from being updated or run
	void disable() {
		set_enable(false);
	}

	// Sets whether the system is enabled or disabled
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

	// Add a system that this system depends on
	void add_dependency(system_base* sys) {
		dependencies.push_back(sys);
	}

	// Clear all stored system dependencies
	void clear_dependencies() {
		dependencies.clear();
	}

	std::span<system_base* const> get_dependencies() const {
		return dependencies;
	}

	void get_flattened_dependencies(std::vector<system_base*>& deps, std::unordered_set<system_base*>& visited) {
		if (visited.contains(this))
			return;

		for (system_base* sys : dependencies)
			sys->get_flattened_dependencies(deps, visited);

		deps.push_back(this);
		visited.insert(this);
	}

	virtual operation make_operation() = 0;

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

private:
	// Other systems that need to be run before this system can safely run
	std::vector<system_base*> dependencies;

	// Whether this system is enabled or disabled. Disabled systems are neither updated nor run.
	bool enabled = true;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_BASE

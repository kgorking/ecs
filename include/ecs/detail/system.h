#ifndef ECS_SYSTEM
#define ECS_SYSTEM

#include <array>
#include <type_traits>
#include <utility>

#include "../entity_id.h"
#include "../entity_range.h"
#include "entity_range.h"
#include "interval_limiter.h"
#include "options.h"
#include "system_base.h"
#include "system_defs.h"
#include "type_hash.h"

namespace ecs::detail {

// The implementation of a system specialized on its components
template <class Options, class UpdateFn, class Pools, bool FirstIsEntity, class ComponentsList>
class system : public system_base {
	virtual void do_run() = 0;
	virtual void do_build() = 0;

public:
	system(UpdateFn func, Pools in_pools) : update_func{func}, pools{in_pools} {
	}

	void run() override {
		if (!is_enabled()) {
			return;
		}

		if (!interval_checker.can_run()) {
			return;
		}

		do_run();

		// Notify pools if data was written to them
		for_each_type<ComponentsList>([this]<typename T>() {
			this->notify_pool_modifed<T>();
		});
	}

	template <typename T>
	void notify_pool_modifed() {
		if constexpr (detail::is_parent<T>::value && !is_read_only<T>()) { // writeable parent
			// Recurse into the parent types
			for_each_type<parent_type_list_t<T>>([this]<typename... ParentTypes>() {
				(this->notify_pool_modifed<ParentTypes>(), ...);
			});
		} else if constexpr (std::is_reference_v<T> && !is_read_only<T>() && !std::is_pointer_v<T>) {
			get_pool<T>(pools).notify_components_modified();
		}
	}

	constexpr int get_group() const noexcept override {
		using group = test_option_type_or<is_group, Options, opts::group<0>>;
		return group::group_id;
	}

	constexpr std::span<detail::type_hash const> get_type_hashes() const noexcept override {
		return type_hashes;
	}

	constexpr bool has_component(detail::type_hash hash) const noexcept override {
		auto const check_hash = [hash]<typename T>() {
			return get_type_hash<T>() == hash;
		};

		if (any_of_type<stripped_component_list>(check_hash))
			return true;

		if constexpr (has_parent_types) {
			return any_of_type<parent_component_list>(check_hash);
		} else {
			return false;
		}
	}

	constexpr bool depends_on(system_base const* other) const noexcept override {
		return any_of_type<stripped_component_list>([this, other]<typename T>() {
			constexpr auto hash = get_type_hash<T>();

			// If the other system doesn't touch the same component,
			// then there can be no dependecy
			if (!other->has_component(hash))
				return false;

			bool const other_writes = other->writes_to_component(hash);
			if (other_writes) {
				// The other system writes to the component,
				// so there is a strong dependency here.
				// Order is preserved.
				return true;
			} else { // 'other' reads component
				bool const this_writes = writes_to_component(hash);
				if (this_writes) {
					// This system writes to the component,
					// so there is a strong dependency here.
					// Order is preserved.
					return true;
				} else {
					// These systems have a weak read/read dependency
					// and can be scheduled concurrently
					// Order does not need to be preserved.
					return false;
				}
			}
		});
	}

	constexpr bool writes_to_component(detail::type_hash hash) const noexcept override {
		auto const check_writes = [hash]<typename T>() {
			return get_type_hash<std::remove_cvref_t<T>>() == hash && !is_read_only<T>();
		};

		if (any_of_type<ComponentsList>(check_writes))
			return true;

		if constexpr (has_parent_types) {
			return any_of_type<parent_component_list>(check_writes);
		} else {
			return false;
		}
	}

protected:
	// Handle changes when the component pools change
	void process_changes(bool force_rebuild) override {
		if (force_rebuild) {
			do_build();
			return;
		}

		if (!is_enabled()) {
			return;
		}

		if (pools.has_component_count_changed()) {
			do_build();
		}
	}

protected:
	// Number of components
	static constexpr size_t num_components = type_list_size<ComponentsList>;

	// List of components used, with all modifiers stripped
	using stripped_component_list = transform_type<ComponentsList, std::remove_cvref_t>;

	using user_interval = test_option_type_or<is_interval, Options, opts::interval<0, 0>>;
	using interval_type =
		std::conditional_t<(user_interval::_ecs_duration > 0.0),
						   interval_limiter<user_interval::_ecs_duration_ms, user_interval::_ecs_duration_us>, no_interval_limiter>;

	//
	// ecs::parent related stuff

	// The parent type, or void
	using full_parent_type = test_option_type_or<is_parent, stripped_component_list, void>;
	using stripped_parent_type = std::remove_pointer_t<std::remove_cvref_t<full_parent_type>>;
	using parent_component_list = parent_type_list_t<stripped_parent_type>;
	static constexpr bool has_parent_types = !std::is_same_v<full_parent_type, void>;


	// Number of filters
	static constexpr size_t num_filters = count_if<ComponentsList>([]<typename T>() { return std::is_pointer_v<T>; });
	static_assert(num_filters < num_components, "systems must have at least one non-filter component");

	// Hashes of stripped types used by this system ('int' instead of 'int const&')
	static constexpr std::array<detail::type_hash, num_components> type_hashes = get_type_hashes_array<stripped_component_list>();

	// The user supplied system
	UpdateFn update_func;

	// Fully typed component pools used by this system
	Pools const pools;

	interval_type interval_checker;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM

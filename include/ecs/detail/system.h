#ifndef ECS_SYSTEM
#define ECS_SYSTEM

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <unordered_set>

#include "../entity_id.h"
#include "../entity_range.h"
#include "component_pool.h"
#include "entity_range.h"
#include "frequency_limiter.h"
#include "options.h"
#include "system_base.h"
#include "system_defs.h"
#include "type_hash.h"

namespace ecs::detail {
// The implementation of a system specialized on its components
template <class Options, class UpdateFn, class TupPools, class FirstComponent, class... Components>
class system : public system_base {
	virtual void do_run() = 0;
	virtual void do_build(entity_range_view) = 0;

public:
	system(UpdateFn update_func, TupPools pools)
		: update_func{update_func}
		, pools{pools}
		, pool_parent_id{nullptr}
	{
		if constexpr (has_parent_types) {
			pool_parent_id = &detail::get_pool<parent_id>(pools);
		}
	}

	void run() override {
		if (!is_enabled()) {
			return;
		}

		if (!frequency.can_run()) {
			return;
		}

		do_run();

		// Notify pools if data was written to them
		if constexpr (!is_entity<FirstComponent>) {
			notify_pool_modifed<FirstComponent>();
		}
		(notify_pool_modifed<Components>(), ...);
	}

	template <typename T>
	void notify_pool_modifed() {
		if constexpr (detail::is_parent<T>::value && !is_read_only<T>()) { // writeable parent
			// Recurse into the parent types
			for_each_type<parent_type_list_t<T>>(
				[this]<typename ...ParentTypes>() { (this->notify_pool_modifed<ParentTypes>(), ...); });
		} else if constexpr (std::is_reference_v<T> && !is_read_only<T>() && !std::is_pointer_v<T>) {
			get_pool<reduce_parent_t<std::remove_cvref_t<T>>>(pools).notify_components_modified();
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
		auto const check_hash = [hash]<typename T>() { return get_type_hash<T>() == hash; };

		if (any_of_type<stripped_component_list>(check_hash))
			return true;

		if constexpr (has_parent_types) {
			return any_of_type<parent_component_list>(check_hash);
		} else {
			return false;
		}
	}

	constexpr bool depends_on(system_base const *other) const noexcept override {
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

		if (any_of_type<component_list>(check_writes))
			return true;

		if constexpr (has_parent_types) {
			return any_of_type<parent_component_list>(check_writes);
		} else {
			return false;
		}
	}

private:
	// Handle changes when the component pools change
	void process_changes(bool force_rebuild) override {
		if (force_rebuild) {
			find_entities();
			return;
		}

		if (!is_enabled()) {
			return;
		}

		bool const modified = std::apply([](auto... pools) { return (pools->has_component_count_changed() || ...); }, pools);

		if (modified) {
			find_entities();
		}
	}

	// Locate all the entities affected by this system
	// and send them to the argument builder
	void find_entities() {
		if constexpr (num_components == 1 && !has_parent_types) {
			// Build the arguments
			entity_range_view const entities = std::get<0>(pools)->get_entities();
			do_build(entities);
		} else {
			// When there are more than one component required for a system,
			// find the intersection of the sets of entities that have those components

			std::vector<entity_range> ranges = find_entity_pool_intersections<FirstComponent, Components...>(pools);

			if constexpr (has_parent_types) {
				// the vector of ranges to remove
				std::vector<entity_range> ents_to_remove;

				for (auto const& range : ranges) {
					for (auto const ent : range) {
						// Get the parent ids in the range
						parent_id const pid = *pool_parent_id->find_component_data(ent);

						// Does tests on the parent sub-components to see they satisfy the constraints
						// ie. a 'parent<int*, float>' will return false if the parent does not have a float or
						// has an int.
						for_each_type<parent_component_list>([&pid, this, ent, &ents_to_remove]<typename T>() {
							// Get the pool of the parent sub-component
							auto const &sub_pool = detail::get_pool<T>(this->pools);

							if constexpr (std::is_pointer_v<T>) {
								// The type is a filter, so the parent is _not_ allowed to have this component
								if (sub_pool.has_entity(pid)) {
									merge_or_add(ents_to_remove, entity_range{ent, ent});
								}
							} else {
								// The parent must have this component
								if (!sub_pool.has_entity(pid)) {
									merge_or_add(ents_to_remove, entity_range{ent, ent});
								}
							}
						});
					}
				}

				// Remove entities from the result
				ranges = difference_ranges(ranges, ents_to_remove);
			}

			do_build(ranges);
		}
	}

protected:
	// The user supplied system
	UpdateFn update_func;

	// A tuple of the fully typed component pools used by this system
	TupPools const pools;

	// The pool that holds 'parent_id's
	component_pool<parent_id> const* pool_parent_id;

	// List of components used
	using component_list = type_list<FirstComponent, Components...>;
	using stripped_component_list = type_list<std::remove_cvref_t<FirstComponent>, std::remove_cvref_t<Components>...>;

	using user_freq = test_option_type_or<is_frequency, Options, opts::frequency<0>>;
	using frequency_type = std::conditional_t<(user_freq::hz > 0),
		frequency_limiter<user_freq::hz>,
		no_frequency_limiter>;
	frequency_type frequency;

	// Number of arguments
	static constexpr size_t num_arguments = 1 + sizeof...(Components);

	// Number of components
	static constexpr size_t num_components = sizeof...(Components) + !is_entity<FirstComponent>;

	// Number of filters
	static constexpr size_t num_filters = (std::is_pointer_v<FirstComponent> + ... + std::is_pointer_v<Components>);
	static_assert(num_filters < num_components, "systems must have at least one non-filter component");

	// Hashes of stripped types used by this system ('int' instead of 'int const&')
	static constexpr std::array<detail::type_hash, num_components> type_hashes =
		get_type_hashes_array<is_entity<FirstComponent>, std::remove_cvref_t<FirstComponent>, std::remove_cvref_t<Components>...>();

	//
	// ecs::parent related stuff

	// The index of potential ecs::parent<> component
	static constexpr int parent_index = test_option_index<is_parent, stripped_component_list>;
	static constexpr bool has_parent_types = (parent_index != -1);

	// The parent type, or void
	using full_parent_type = type_list_at_or<parent_index, component_list, void>;
	using stripped_parent_type = std::remove_cvref_t<full_parent_type>;
	using parent_component_list = parent_type_list_t<stripped_parent_type>;
	static constexpr int num_parent_components = type_list_size<parent_component_list>;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM

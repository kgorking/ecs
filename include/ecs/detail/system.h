#ifndef __SYSTEM
#define __SYSTEM

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

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
	system(UpdateFn update_func, TupPools pools) : update_func{update_func}, pools{pools} {}

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
			get_pool<reduce_parent_t<std::remove_cvref_t<T>>>().notify_components_modified();
		}
	}

	constexpr int get_group() const noexcept override {
		using group = test_option_type_or<is_group, Options, opts::group<0>>;
		return group::group_id;
	}

	std::string get_signature() const noexcept override {
		// Component names
		constexpr std::array<std::string_view, num_arguments> argument_names{get_type_name<FirstComponent>(),
																			 get_type_name<Components>()...};

		std::string sig("system(");
		for (size_t i = 0; i < num_arguments - 1; i++) {
			sig += argument_names[i];
			sig += ", ";
		}
		sig += argument_names[num_arguments - 1];
		sig += ')';
		return sig;
	}

	constexpr std::span<detail::type_hash const> get_type_hashes() const noexcept override {
		return type_hashes;
	}

	constexpr bool has_component(detail::type_hash hash) const noexcept override {
		return any_of_type<stripped_component_list>([hash]<typename T>() {
			return get_type_hash<T>() == hash;
		});
		//return type_hashes.end() != std::find(type_hashes.begin(), type_hashes.end(), hash);
	}

	constexpr bool depends_on(system_base const *other) const noexcept override {
		for (auto hash : get_type_hashes()) {
			// If the other system doesn't touch the same component,
			// then there can be no dependecy
			if (!other->has_component(hash))
				continue;

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
					continue;
				}
			}
		}

		return false;
	}

	constexpr bool writes_to_component(detail::type_hash hash) const noexcept override {
		auto const it = std::find(type_hashes.begin(), type_hashes.end(), hash);
		if (it == type_hashes.end())
			return false;

		// Contains true if a type is read-only
		constexpr std::array<bool, num_components> type_read_only =
			get_type_read_only<is_entity<FirstComponent>, FirstComponent, Components...>();

		return !type_read_only[std::distance(type_hashes.begin(), it)];
	}

	template <bool ignore_first_arg, typename First, typename... Types>
	static constexpr auto get_type_read_only() {
		if constexpr (!ignore_first_arg) {
			return std::array<bool, 1 + sizeof...(Types)>{is_read_only<First>(), is_read_only<Types>()...};
		} else {
			return std::array<bool, sizeof...(Types)>{is_read_only<Types>()...};
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
		if constexpr (num_components == 1) {
			// Build the arguments
			entity_range_view const entities = std::get<0>(pools)->get_entities();
			do_build(entities);
		} else {
			// When there are more than one component required for a system,
			// find the intersection of the sets of entities that have those components

			// Build the arguments
			auto const ranges = find_entity_pool_intersections<FirstComponent, Components...>(pools);
			do_build(ranges);
		}
	}

	template <typename Component>
	[[nodiscard]] component_pool<Component> &get_pool() const {
		return detail::get_pool<Component>(pools);
	}

protected:
	// The user supplied system
	UpdateFn update_func;

	// A tuple of the fully typed component pools used by this system
	TupPools const pools;

	// List of components used
	using component_list = type_list<FirstComponent, Components...>;
	using stripped_component_list = type_list<std::remove_cvref_t<FirstComponent>, std::remove_cvref_t<Components>...>;

	using user_freq = test_option_type_or<is_frequency, Options, opts::frequency<0>>;
	frequency_limiter<user_freq::hz> frequency;

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
	static constexpr bool has_parent = (parent_index != -1);

	// The parent type, or void
	using full_parent_type = type_list_at_or<parent_index, component_list, void>;
	using stripped_parent_type = std::remove_cvref_t<full_parent_type>;
	using parent_component_list = parent_type_list_t<stripped_parent_type>;
	static constexpr int num_parent_components = type_list_size<parent_component_list>;
};
} // namespace ecs::detail

#endif // !__SYSTEM

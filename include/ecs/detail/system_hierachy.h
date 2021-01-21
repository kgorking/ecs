#ifndef __SYSTEM_HIERARCHY_H_
#define __SYSTEM_HIERARCHY_H_

#include <map>
#include <unordered_map>

#include "../parent.h"
#include "entity_range_iterator.h"
#include "find_entity_pool_intersections.h"
#include "pool_entity_walker.h"
#include "system.h"
#include "system_defs.h"

namespace ecs::detail {
template <class Options, class UpdateFn, class TupPools, class FirstComponent, class... Components>
class system_hierarchy final : public system<Options, UpdateFn, TupPools, FirstComponent, Components...> {
	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

	using entity_info = std::pair<int, entity_type>; // parent count, root id
	using info_map = std::unordered_map<entity_type, entity_info>;
	using info_iterator = info_map::iterator;

	using argument = decltype(
		std::tuple_cat(std::tuple<entity_id>{0}, std::declval<argument_tuple<FirstComponent, Components...>>(), std::tuple<entity_info>{}));

public:
	system_hierarchy(UpdateFn update_func, TupPools pools)
		: system<Options, UpdateFn, TupPools, FirstComponent, Components...>{update_func, pools},
		  parent_pools{std::apply([&pools](auto... parent_types) { return std::make_tuple(&get_pool<decltype(parent_types)>(pools)...); },
								  parent_types_tuple_t<parent_type>{})},
		  pool_parent_id{get_pool<parent_id>(pools)}, walker(pools) {}

private:
	void do_run() override {
		auto const e_p = execution_policy{}; // cannot pass directly to 'for_each' in gcc
		std::for_each(e_p, argument_spans.begin(), argument_spans.end(), [this](auto const local_span) {
			for (argument &arg : local_span) {
				if constexpr (is_entity<FirstComponent>) {
					this->update_func(std::get<entity_id>(arg), extract<Components>(arg)...);
				} else {
					this->update_func(extract<FirstComponent>(arg), extract<Components>(arg)...);
				}
			}
		});
	}

	// Extracts a component argument from a tuple
	template <typename Component, typename Tuple>
	static decltype(auto) extract(Tuple &tuple) {
		using T = std::remove_cvref_t<Component>;

		if constexpr (std::is_pointer_v<T>) {
			return nullptr;
		} else if constexpr (detail::is_parent<T>::value) {
			return std::get<T>(tuple);
		} else {
			T *ptr = std::get<T *>(tuple);
			return *(ptr);
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build(entity_range_view ranges) override {
		// Clear the arguments
		arguments.clear();
		argument_spans.clear();

		if (ranges.size() == 0) {
			return;
		}

		// Reset the walker to the new entities
		walker.reset(ranges);

		// map of entity info
		info_map info;

		// Build the arguments for the ranges
		// TODO optimize this further
		int index = 0;
		while (!walker.done()) {
			entity_id const entity = walker.get_entity();

			// TODO move this check to system::find_entities
			if constexpr (has_parent_types()) {
				if (!has_required_parent_types(entity)) {
					walker.next();
					continue;
				}
			}

			info_iterator const ent_info = fill_entity_info(info, entity, index);

			// Add the argument for the entity
			if constexpr (is_entity<FirstComponent>) {
				arguments.emplace_back(entity, walker.template get<Components>()..., ent_info->second);
			} else {
				arguments.emplace_back(entity, walker.template get<FirstComponent>(), walker.template get<Components>()...,
									   ent_info->second);
			}

			// Update the spans
			size_t const root_index = ent_info->second.second;
			if (root_index == argument_spans.size()) {
				// New root of a tree has been started, so create its new span with a count of one
				argument_spans.emplace_back((argument *)nullptr, 1);
			} else {
				// Update the child-count of an existing tree
				auto& current_span = argument_spans[root_index];
				current_span = std::span((argument *)nullptr, 1 + current_span.size());
			}

			walker.next();
		}

		std::sort(std::execution::par, arguments.begin(), arguments.end(), topological_sort_func);

		// Update the offsets in the spans
		// TODO collapse to std::thread::hardware_concurrency() spans?
		size_t count = 0;
		for (std::span<argument> &current_span : argument_spans) {
			current_span = std::span(arguments.data() + count, current_span.size());
			count += current_span.size();
		}
	}

	static bool topological_sort_func(argument const &arg_l, argument const &arg_r) {
		auto const &[depth_l, root_l] = std::get<entity_info>(arg_l);
		auto const &[depth_r, root_r] = std::get<entity_info>(arg_r);

		// order by roots
		if (root_l != root_r)
			return root_l < root_r;
		else
			// order by depth
			return depth_l < depth_r;
	}

	info_iterator fill_entity_info(info_map &info, entity_id const entity, int &index) {
		auto const ent_it = info.find(entity);
		if (ent_it != info.end())
			return ent_it;

		// Get the parent id
		entity_id const *parent_id = pool_parent_id.find_component_data(entity);
		if (parent_id == nullptr) {
			// This entity does not have a 'parent_id' component,
			// which means that this entity is a root
			auto const [it, _] = info.emplace(std::make_pair(entity, entity_info{0, index++}));
			return it;
		}

		// look up the parent info
		info_iterator const parent_it = fill_entity_info(info, *parent_id, index);

		// insert the entity info
		auto const &[count, root_index] = parent_it->second;
		auto const [it, $] = info.emplace(std::make_pair(entity, entity_info{1 + count, root_index}));
		return it;
	}

	constexpr static bool has_parent_types() {
		return (0 != std::tuple_size_v<decltype(parent_pools)>);
	}

	bool has_required_parent_types(entity_id const entity) const {
		// If the parent has sub-components specified, verify them
		if constexpr (0 != std::tuple_size_v<decltype(parent_pools)>) {
			// Get the parent components id
			parent_id const pid = *pool_parent_id.find_component_data(entity);

			// Does tests on the parent sub-components to see they satisfy the constraints
			// ie. a 'parent<int*, float>' will return false if the parent does not have a float or
			// has an int.
			constexpr parent_types_tuple_t<parent_type> ptt{};
			bool const has_parent_types = std::apply(
				[&](auto const &...parent_types) {
					auto const check_parent = [&](auto parent_type) {
						// Get the pool of the parent sub-component
						auto const &sub_pool = get_pool<decltype(parent_type)>(this->pools);

						if constexpr (std::is_pointer_v<decltype(parent_type)>) {
							// The type is a filter, so the parent is _not_ allowed to have this component
							return !sub_pool.has_entity(pid);
						} else {
							// The parent must have this component
							return sub_pool.has_entity(pid);
						}
					};

					return (check_parent(parent_types) && ...);
				},
				ptt);

			return has_parent_types;
		} else {
			return true;
		}
	}

private:
	// Extract the parent type
	static constexpr int ParentIndex =
		test_option_index<is_parent, std::tuple<std::remove_cvref_t<FirstComponent>, std::remove_cvref_t<Components>...>>;
	static_assert(-1 != ParentIndex, "no parent component found");

	using full_parent_type = std::tuple_element_t<ParentIndex, std::tuple<FirstComponent, Components...>>;
	using parent_type = std::remove_cvref_t<full_parent_type>;

	// The vector of unrolled arguments
	std::vector<argument> arguments;

	// The spans over each tree in the argument vector
	std::vector<std::span<argument>> argument_spans;

	// A tuple of the fully typed component pools used the parent component
	parent_pool_tuple_t<parent_type> const parent_pools;

	// The pool that holds 'parent_id's
	component_pool<parent_id> const &pool_parent_id;

	// walker
	using walker_type = std::conditional_t<is_entity<FirstComponent>, pool_entity_walker<TupPools, Components...>,
										   pool_entity_walker<TupPools, FirstComponent, Components...>>;
	walker_type walker;
};
} // namespace ecs::detail

#endif // !__SYSTEM_HIERARCHY_H_

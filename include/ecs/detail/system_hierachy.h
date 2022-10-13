#ifndef ECS_SYSTEM_HIERARCHY_H_
#define ECS_SYSTEM_HIERARCHY_H_

#include "tls/collect.h"

#include "../parent.h"
#include "entity_offset.h"
#include "find_entity_pool_intersections.h"
#include "system.h"
#include "system_defs.h"
#include "type_list.h"

namespace ecs::detail {
template <typename Options, typename UpdateFn, typename TupPools, bool FirstIsEntity, typename ComponentsList>
class system_hierarchy final : public system<Options, UpdateFn, TupPools, FirstIsEntity, ComponentsList> {
	using base = system<Options, UpdateFn, TupPools, FirstIsEntity, ComponentsList>;

	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

public:
	system_hierarchy(UpdateFn func, TupPools in_pools)
		: base{func, in_pools}, parent_pools{make_parent_types_tuple()} {
		pool_parent_id = &detail::get_pool<parent_id>(this->pools);
		this->process_changes(true);
	}

private:
	void do_run() override {
		auto const this_pools = this->pools;
		for(entity_info const& info : infos) {
			arguments[info.range_index](this->update_func, info.offset, this_pools);
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build() override {
		std::vector<entity_range> ranges;
		std::vector<entity_range> ents_to_remove;

		// Find the entities
		find_entity_pool_intersections_cb<ComponentsList>(this->pools, [&](entity_range range) {
			ranges.push_back(range);

			// Get the parent ids in the range
			parent_id const* pid_ptr = pool_parent_id->find_component_data(range.first());

			// the ranges to remove
			for_each_type<parent_component_list>([this, pid_ptr, range, &ents_to_remove]<typename T>() {
				// Get the pool of the parent sub-component
				using X = std::remove_pointer_t<T>;
				component_pool<X> const& sub_pool = this->pools.template get<X>();

				for (size_t pid_index = 0; entity_id const ent : range) {
					parent_id const pid = pid_ptr[pid_index/*range.offset(ent)*/];
					pid_index += 1;

					// Does tests on the parent sub-components to see they satisfy the constraints
					// ie. a 'parent<int*, float>' will return false if the parent does not have a float or
					// has an int.
					if (std::is_pointer_v<T> == sub_pool.has_entity(pid)) {
						// Above 'if' statement is the same as
						//   if (!pointer && !has_entity)	// The type is a filter, so the parent is _not_ allowed to have this component
						//   if (pointer && has_entity)		// The parent must have this component
						merge_or_add(ents_to_remove, entity_range{ent, ent});
					}
				}
			});
		});

		// Remove entities from the result
		ranges = difference_ranges(ranges, ents_to_remove);

		// Clear the arguments
		arguments.clear();
		infos.clear();
		//argument_spans.clear();

		if (ranges.empty()) {
			return;
		}

		// Build the arguments for the ranges
		apply_type<ComponentsList>([&]<typename... T>() {
			for (int index = 0; entity_range const range : ranges) {
				arguments.emplace_back(make_argument<T...>(range, get_component<T>(range.first(), this->pools)...));

				for (entity_id const id : range) {
					infos.emplace_back(entity_info{0, *(pool_parent_id->find_component_data(id)), index, range.offset(id)});
				}

				index += 1;
			}
		});

		// partition the roots
		auto begin = std::partition(infos.begin(), infos.end(), [&](entity_info const& info) {
			return false == pool_parent_id->has_entity(info.root_id);
		});

		// data needed by the partition lambda
		auto prev_begin = infos.begin();
		int hierarchy_level = 1;

		// The lambda used to partion non-root entities
		const auto parter = [&](entity_info& info) {
			// update the parent count while we are here anyway
			info.parent_count = hierarchy_level;

			auto const it = std::find_if(prev_begin, begin, [&](entity_info const& parent_info) {
				entity_id const parent_id = ranges[parent_info.range_index].at(parent_info.offset);
				return parent_id == info.root_id;
			});

			if (begin != it) {
				// Propagate the root id downwards to its children
				info.root_id = it->root_id;

				// A parent was found in the previous partition
				return true;
			}
			return false;
		};

		// partition the levels below the roots
		while (begin != infos.end()) {
			auto new_begin = std::partition(begin, infos.end(), parter);

			if (new_begin == begin)
				break;

			prev_begin = begin;
			begin = new_begin;

			hierarchy_level += 1;
		}

		// Do the topological sort of the arguments
		//std::sort(infos.begin(), infos.end());
	}

	template <typename... Ts>
	static auto make_argument(entity_range range, auto... args) noexcept {
		return [=](auto update_func, entity_offset offset, auto& pools) mutable {
			entity_id const ent = range.first() + offset;
			if constexpr (FirstIsEntity) {
				update_func(ent, extract_arg_lambda<Ts>(args, offset, pools)...);
			} else {
				update_func(/**/ extract_arg_lambda<Ts>(args, offset, pools)...);
			}
		};
	}

	decltype(auto) make_parent_types_tuple() const {
		return apply_type<parent_component_list>([this]<typename... T>() {
			return std::make_tuple(&get_pool<std::remove_pointer_t<T>>(this->pools)...);
		});
	}

private:
	using base::has_parent_types;
	using typename base::full_parent_type;
	using typename base::parent_component_list;
	using typename base::stripped_component_list;
	using typename base::stripped_parent_type;

	// The argument for parameter to pass to system func
	using base_argument = decltype(apply_type<ComponentsList>([]<typename... Types>() {
		return make_argument<Types...>(entity_range{0, 0}, component_argument<Types>{0}...);
	}));

	// Ensure we have a parent type
	static_assert(has_parent_types, "no parent component found");

	struct entity_info {
		int parent_count;
		entity_type root_id;

		int range_index;
		entity_offset offset;

		auto operator<=>(entity_info const&) const = default;
	};

	// The vector of entity/parent info
	std::vector<entity_info> infos;

	// The vector of unrolled arguments
	std::vector<std::remove_const_t<base_argument>> arguments;

	// The spans over each tree in the argument vector
	//std::vector<std::span<argument>> argument_spans;

	// The pool that holds 'parent_id's
	component_pool<parent_id> const* pool_parent_id;

	// A tuple of the fully typed component pools used the parent component
	parent_pool_tuple_t<stripped_parent_type> const parent_pools;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_HIERARCHY_H_

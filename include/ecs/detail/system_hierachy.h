#ifndef ECS_SYSTEM_HIERARCHY_H_
#define ECS_SYSTEM_HIERARCHY_H_

#include "../parent.h"
#include "find_entity_pool_intersections.h"
#include "system.h"
#include "system_defs.h"
#include "type_list.h"

namespace ecs::detail {
template <typename Options, typename UpdateFn, bool FirstIsEntity, typename ComponentsList, typename CombinedList, typename PoolsList>
class system_hierarchy final : public system<Options, UpdateFn, FirstIsEntity, CombinedList, PoolsList> {
	using base = system<Options, UpdateFn, FirstIsEntity, CombinedList, PoolsList>;

	// Is parallel execution wanted
	static constexpr bool is_parallel = !ecs::detail::has_option<opts::not_parallel, Options>();

	struct location {
		std::uint32_t index;
		entity_offset offset;
		auto operator<=>(location const&) const = default;
	};
	struct entity_info {
		std::uint32_t parent_count;
		entity_type root_id;
		location l;

		auto operator<=>(entity_info const&) const = default;
	};
	struct hierarchy_span {
		unsigned offset, count;
	};

public:
	system_hierarchy(UpdateFn func, component_pools<PoolsList>&& in_pools)
		: base{func, std::forward<component_pools<PoolsList>>(in_pools)} {
		pool_parent_id = &this->pools.template get<parent_id>();
		this->process_changes(true);
	}

private:
	operation make_operation() override {
		return operation{(base_argument*)0, this->get_update_func()};
	}

	void do_run() override {
		if constexpr (is_parallel) {
			std::for_each(std::execution::par, info_spans.begin(), info_spans.end(), [&](hierarchy_span span) {
				auto const ei_span = std::span<entity_info>{infos.data() + span.offset, span.count};
				for (entity_info const& info : ei_span) {
					arguments[info.l.index](this->update_func, 0, info.l.offset);
				}
			});
		} else {
			for (entity_info const& info : infos) {
				arguments[info.l.index](this->update_func, 0, info.l.offset);
			}
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build() override {
		ranges.clear();
		ents_to_remove.clear();

		// Find the entities
		find_entity_pool_intersections_cb<ComponentsList>(this->pools, [&](entity_range const range) {
			ranges.push_back(range);

			// Get the parent ids in the range
			parent_id const* pid_ptr = pool_parent_id->find_component_data(range.first());

			// the ranges to remove
			for_each_type<parent_component_list>([&]<typename T>() {
				// Get the pool of the parent sub-component
				using X = std::remove_pointer_t<T>;
				component_pool<X> const& sub_pool = this->pools.template get<X>();

				size_t pid_index = 0;
				for (entity_id const ent : range) {
					parent_id const pid = pid_ptr[pid_index];
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
		info_spans.clear();

		if (ranges.empty()) {
			return;
		}

		// Build the arguments for the ranges
		for_all_types<ComponentsList>([&]<typename... T>() {
			for (unsigned index = 0; entity_range const range : ranges) {
				arguments.push_back(make_argument<T...>(range, &(this->pools), get_component<T>(range.first(), this->pools)...));

				for (entity_id const id : range) {
					infos.push_back({0, *(pool_parent_id->find_component_data(id)), {index, range.offset(id)}});
				}

				index += 1;
			}
		});

		// partition the roots
		auto it = std::partition(infos.begin(), infos.end(), [&](entity_info const& info) {
			return false == pool_parent_id->has_entity(info.root_id);
		});

		// Keep partitioning if there are more levels in the hierarchies
		if (it != infos.begin()) {
			// data needed by the partition lambda
			auto prev_it = infos.begin();
			unsigned hierarchy_level = 1;

			// The lambda used to partion non-root entities
			const auto parter = [&](entity_info& info) {
				// update the parent count while we are here anyway
				info.parent_count = hierarchy_level;

				// Look for the parent in the previous partition
				auto const parent_it = std::find_if(prev_it, it, [&](entity_info const& parent_info) {
					entity_id const parent_id = ranges[parent_info.l.index].at(parent_info.l.offset);
					return parent_id == info.root_id;
				});

				if (it != parent_it) {
					// Propagate the root id downwards to its children
					info.root_id = parent_it->root_id;

					// A parent was found in the previous partition
					return true;
				}
				return false;
			};

			// partition the levels below the roots
			while (it != infos.end()) {
				auto next_it = std::partition(it, infos.end(), parter);

				// Nothing was partitioned, so leave
				if (next_it == it)
					break;

				// Update the partition iterators
				prev_it = it;
				it = next_it;

				hierarchy_level += 1;
			}
		}

		// Do the topological sort of the arguments
		std::sort(infos.begin(), infos.end());

		// The spans are only needed for parallel execution
		if constexpr (is_parallel) {
			// Create the spans
			auto current_root = infos.front().root_id;
			unsigned count = 1;
			unsigned offset = 0;

			for (size_t i = 1; i < infos.size(); i++) {
				entity_info const& info = infos[i];
				if (current_root != info.root_id) {
					info_spans.push_back({offset, count});

					current_root = info.root_id;
					offset += count;
					count = 0;
				}

				// TODO compress 'infos' into 'location's inplace

				count += 1;
			}
			info_spans.push_back({offset, static_cast<unsigned>(infos.size() - offset)});
		}
	}

	template <typename... Ts>
	static auto make_argument(entity_range range, component_pools<PoolsList> const* pools, auto... args) noexcept {
		return [=](auto update_func, entity_id, entity_offset offset) mutable {
			entity_id const ent = static_cast<entity_type>(static_cast<entity_offset>(range.first()) + offset);
			if constexpr (FirstIsEntity) {
				update_func(ent, extract_arg_lambda<Ts>(args, offset, *pools)...);
			} else {
				update_func(/**/ extract_arg_lambda<Ts>(args, offset, *pools)...);
			}
		};
	}

private:
	using base::has_parent_types;
	using typename base::full_parent_type;
	using typename base::parent_component_list;
	using typename base::stripped_component_list;
	using typename base::stripped_parent_type;

	// The argument for parameter to pass to system func
	using base_argument_ptr = decltype(for_all_types<ComponentsList>([]<typename... Types>() {
		return make_argument<Types...>(entity_range{0, 0}, static_cast<component_pools<PoolsList> const*>(nullptr),
									   component_argument<Types>{0}...);
	}));
	using base_argument = std::remove_const_t<base_argument_ptr>;

	// Ensure we have a parent type
	static_assert(has_parent_types, "no parent component found");

	// The vector of entity/parent info
	std::vector<entity_info> infos;

	// The vector of unrolled arguments
	std::vector<base_argument> arguments;

	// The spans over each tree in the argument vector.
	// Only used for parallel execution
	std::vector<hierarchy_span> info_spans;

	std::vector<entity_range> ranges;
	std::vector<entity_range> ents_to_remove;

	// The pool that holds 'parent_id's
	component_pool<parent_id> const* pool_parent_id;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_HIERARCHY_H_

#ifndef ECS_SYSTEM_SORTED_H_
#define ECS_SYSTEM_SORTED_H_

#include "system.h"
#include "verification.h"

namespace ecs::detail {
// Manages sorted arguments. Neither cache- nor storage space friendly, but arguments
// will be passed to the user supplied lambda in a sorted manner
template <typename Options, typename UpdateFn, typename SortFunc, bool FirstIsEntity, typename ComponentsList, typename PoolsList>
struct system_sorted final : public system<Options, UpdateFn, FirstIsEntity, ComponentsList, PoolsList> {
	using base = system<Options, UpdateFn, FirstIsEntity, ComponentsList, PoolsList>;

	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

public:
	system_sorted(UpdateFn func, SortFunc sort, component_pools<PoolsList>&& in_pools)
		: base{func, std::forward<component_pools<PoolsList>>(in_pools)}, sort_func{sort} {
		this->process_changes(true);
	}

private:
	void do_run() override {
		// Sort the arguments if the component data has been modified
		if (needs_sorting || this->pools.template get<sort_types>().has_components_been_modified()) {
			auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
			std::sort(e_p, sorted_args.begin(), sorted_args.end(), [this](sort_help const& l, sort_help const& r) {
				return sort_func(*l.sort_val_ptr, *r.sort_val_ptr);
			});

			needs_sorting = false;
		}

		if constexpr (FirstIsEntity) {
			for (sort_help const& sh : sorted_args) {
				auto& [range, arg] = arguments[sh.arg_index];
				entity_id const ent = range.at(sh.offset);
				arg(ent, this->update_func, sh.offset);
			}
		} else {
			for (sort_help const& sh : sorted_args) {
				arguments[sh.arg_index].arg(this->update_func, sh.offset);
			}
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build() override {
		sorted_args.clear();
		arguments.clear();

		for_all_types<ComponentsList>([&]<typename... Types>() {
			find_entity_pool_intersections_cb<ComponentsList>(this->pools, [this, index = 0u](entity_range range) mutable {
				arguments.emplace_back(range, make_argument<Types...>(get_component<Types>(range.first(), this->pools)...));

				for (entity_id const entity : range) {
					entity_offset const offset = range.offset(entity);
					sorted_args.push_back({index, offset, get_component<sort_types>(entity, this->pools)});
				}

				index += 1;
			});
		});

		needs_sorting = true;
	}

	template <typename... Ts>
	static auto make_argument(auto... args) {
		if constexpr (FirstIsEntity) {
			return [=](entity_id const ent, auto update_func, entity_offset offset) {
				update_func(ent, extract_arg_lambda<Ts>(args, offset, 0)...);
			};
		} else {
			return [=](auto update_func, entity_offset offset) {
				update_func(extract_arg_lambda<Ts>(args, offset, 0)...);
			};
		}
	}

private:
	// The user supplied sorting function
	SortFunc sort_func;

	// The type used for sorting
	using sort_types = sorter_predicate_type_t<SortFunc>;

	// True if the data needs to be sorted
	bool needs_sorting = false;

	struct sort_help {
		unsigned arg_index;
		entity_offset offset;
		sort_types* sort_val_ptr;
	};
	std::vector<sort_help> sorted_args;

	using argument = std::remove_const_t<decltype(
		for_all_types<ComponentsList>([]<typename... Types>() {
			return make_argument<Types...>(component_argument<Types>{}...);
		}
	))>;

	struct range_argument {
		entity_range range;
		argument arg;
	};

	std::vector<range_argument> arguments;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_SORTED_H_

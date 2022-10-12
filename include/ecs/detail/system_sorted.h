#ifndef ECS_SYSTEM_SORTED_H_
#define ECS_SYSTEM_SORTED_H_

#include "system.h"
#include "verification.h"

namespace ecs::detail {
// Manages sorted arguments. Neither cache- nor storage space friendly, but arguments
// will be passed to the user supplied lambda in a sorted manner
template <typename Options, typename UpdateFn, typename SortFunc, typename TupPools, bool FirstIsEntity, typename ComponentsList>
struct system_sorted final : public system<Options, UpdateFn, TupPools, FirstIsEntity, ComponentsList> {
	using base = system<Options, UpdateFn, TupPools, FirstIsEntity, ComponentsList>;

	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

public:
	system_sorted(UpdateFn func, SortFunc sort, TupPools in_pools)
		: base(func, in_pools), sort_func{sort} {
		this->process_changes(true);
	}

private:
	void do_run() override {
		auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc

		// Sort the arguments if the component data has been modified
		if (needs_sorting || this->pools.template get<sort_types>().has_components_been_modified()) {
			std::sort(e_p, sorted_args.begin(), sorted_args.end(), [this](sort_help const& l, sort_help const& r) {
				return sort_func(*l.sort_val_ptr, *r.sort_val_ptr);
			});

			needs_sorting = false;
		}

		for (sort_help const& sh : sorted_args) {
			lambda_arguments[sh.arg_index](this->update_func, sh.offset);
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build() override {
		sorted_args.clear();
		lambda_arguments.clear();

		apply_type<ComponentsList>([&]<typename... Types>() {
			find_entity_pool_intersections_cb<ComponentsList>(this->pools, [this, index = 0u](entity_range range) mutable {
				lambda_arguments.emplace_back(make_argument<Types...>(range, get_component<Types>(range.first(), this->pools)...));

				for (entity_id const entity : range) {
					entity_offset const offset = range.offset(entity);
					sorted_args.emplace_back(index, offset, get_component<sort_types>(entity, this->pools));
				}

				index += 1;
			});
		});

		needs_sorting = true;
	}

	template <typename... Ts>
	static auto make_argument(entity_range range, auto... args) {
		return [=](auto update_func, entity_offset offset) {
			entity_id const ent = range.first() + offset;
			if constexpr (FirstIsEntity) {
				update_func(ent, extract_arg_lambda<Ts>(args, offset, 0)...);
			} else {
				update_func(/**/ extract_arg_lambda<Ts>(args, offset, 0)...);
			}
		};
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

	using base_argument = decltype(apply_type<ComponentsList>([]<typename... Types>() {
			return make_argument<Types...>(entity_range{0,0}, component_argument<Types>{}...);
		}));
	
	std::vector<std::remove_const_t<base_argument>> lambda_arguments;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_SORTED_H_

#ifndef ECS_SYSTEM_SORTED_H_
#define ECS_SYSTEM_SORTED_H_

#include "system.h"
#include "verification.h"

namespace ecs::detail {
// Manages sorted arguments. Neither cache- nor storage space friendly, but arguments
// will be passed to the user supplied lambda in a sorted manner
template <typename Options, typename UpdateFn, typename SortFunc, class TupPools, bool FirstIsEntity, class ComponentsList>
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
		// Sort the arguments if the component data has been modified
		if (needs_sorting || std::get<pool<sort_types>>(this->pools)->has_components_been_modified()) {
			auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
			std::sort(e_p, arguments.begin(), arguments.end(), [this](auto const& l, auto const& r) {
				sort_types* t_l = std::get<sort_types*>(l);
				sort_types* t_r = std::get<sort_types*>(r);
				return sort_func(*t_l, *t_r);
			});

			needs_sorting = false;
		}

		auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
		std::for_each(e_p, arguments.begin(), arguments.end(), [this](auto packed_arg) {
			apply_type<ComponentsList>([&]<typename... Types>(){
				if constexpr (FirstIsEntity) {
					this->update_func(std::get<0>(packed_arg), extract_arg<Types>(packed_arg, 0)...);
				} else {
					this->update_func(/*                    */ extract_arg<Types>(packed_arg, 0)...);
				}
			});
		});
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build() override {
		arguments.clear();

		find_entity_pool_intersections_cb<ComponentsList>(this->pools, [this](entity_range range) {
			for (entity_id const& entity : range) {
				apply_type<ComponentsList>([&]<typename... Comps>() {
					arguments.emplace_back(entity, get_component<Comps>(entity, this->pools)...);
				});
			}
		});

		needs_sorting = true;
	}

private:
	// The user supplied sorting function
	SortFunc sort_func;

	// The vector of unrolled arguments, sorted using 'sort_func'
	template <typename... Types>
	using tuple_from_types = std::tuple<entity_id, component_argument<Types>...>;
	using argument = transform_type_all<ComponentsList, tuple_from_types>;
	std::vector<argument> arguments;

	// True if the data needs to be sorted
	bool needs_sorting = false;

	using sort_types = sorter_predicate_type_t<SortFunc>;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_SORTED_H_

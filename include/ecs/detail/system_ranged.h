#ifndef ECS_DETAIL_SYSTEM_RANGED_H
#define ECS_DETAIL_SYSTEM_RANGED_H

#include "system.h"

namespace ecs::detail {
// Manages arguments using ranges. Very fast linear traversal and minimal storage overhead.
template <typename Options, typename UpdateFn, bool FirstIsEntity, typename ComponentsList, typename PoolsList>
class system_ranged final : public system<Options, UpdateFn, FirstIsEntity, ComponentsList, PoolsList> {
	using base = system<Options, UpdateFn, FirstIsEntity, ComponentsList, PoolsList>;

	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

public:
	system_ranged(UpdateFn func, component_pools<PoolsList>&& in_pools)
		: base{func, std::forward<component_pools<PoolsList>>(in_pools)} {
		this->process_changes(true);
	}

private:
	void do_run() override {
		// Call the system for all the components that match the system signature
		for (auto& argument : lambda_arguments) {
			argument(this->update_func);
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build() override {
		// Clear current arguments
		lambda_arguments.clear();

		for_all_types<ComponentsList>([&]<typename... Type>() {
			find_entity_pool_intersections_cb<ComponentsList>(this->pools, [this](entity_range found_range) {
				lambda_arguments.push_back(make_argument<Type...>(found_range, get_component<Type>(found_range.first(), this->pools)...));
			});
		});
	}

	template <typename... Ts>
	static auto make_argument(entity_range const range, auto... args) {
		return [=](auto update_func) noexcept {
			auto constexpr e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
			std::for_each(e_p, range.begin(), range.end(), [=](entity_id const ent) mutable noexcept {
				auto const offset = ent - range.first();

				if constexpr (FirstIsEntity) {
					update_func(ent, extract_arg_lambda<Ts>(args, offset, 0)...);
				} else {
					update_func(/**/ extract_arg_lambda<Ts>(args, offset, 0)...);
				}
			});
		};
	}

private:
	/// XXX
	using base_argument = decltype(for_all_types<ComponentsList>([]<typename... Types>() {
			return make_argument<Types...>(entity_range{0,0}, component_argument<Types>{}...);
		}));
	
	std::vector<std::remove_const_t<base_argument>> lambda_arguments;

};
} // namespace ecs::detail

#endif // !ECS_DETAIL_SYSTEM_RANGED_H

#ifndef ECS_SYSTEM_RANGED_H_
#define ECS_SYSTEM_RANGED_H_

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
		for (auto& [range, argument] : range_arguments) {
			argument(range, this->update_func);
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build() override {
		// Clear current arguments
		range_arguments.clear();

		for_all_types<ComponentsList>([&]<typename... Type>() {
			find_entity_pool_intersections_cb<ComponentsList>(this->pools, [this](entity_range found_range) {
				range_arguments.emplace_back(found_range,
											 make_argument<Types...>(get_component<Types>(found_range.first(), this->pools)...));
			});
		});
	}

	template <typename... Ts>
	static auto make_argument(auto... args) {
		if constexpr (FirstIsEntity) {
			return [=](entity_range const range, auto update_func) {
				entity_offset offset = 0;
				for (entity_id const ent : range) {
					update_func(ent, extract_arg_lambda<Ts>(args, offset, 0)...);
					offset += 1;
				}
			};
		} else {
			return [=](entity_range const range, auto update_func) {
				for (entity_offset offset = 0; offset < range.count(); ++offset) {
					update_func(extract_arg_lambda<Ts>(args, offset, 0)...);
				}
			};
		}
	}

private:
	// Get the type of lambda containing the arguments
	using argument = std::remove_const_t<decltype(
		for_all_types<ComponentsList>([]<typename... Types>() {
			return make_argument<Types...>(component_argument<Types>{}...);
		}
	))>;

	struct range_argument {
		entity_range range;
		argument arg;
	};
	
	std::vector<range_argument> range_arguments;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_RANGED_H_

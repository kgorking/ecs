#ifndef ECS_SYSTEM_RANGED_H_
#define ECS_SYSTEM_RANGED_H_

#include "system.h"
#include "static_scheduler.h"

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
		for (std::size_t i = 0; i < ranges.size(); i++) {
			auto const& range = ranges[i];
			auto& arg = arguments[i];

			operation const op(arg, this->get_update_func());

			for (entity_offset offset = 0; offset < range.count(); ++offset) {
				op.run(range.first() + offset, offset);
				//arg(this->get_update_func(), range.first() + offset, offset);
			}
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build() override {
		// Clear current arguments
		ranges.clear();
		arguments.clear();

		for_all_types<ComponentsList>([&]<typename... Types>() {
			find_entity_pool_intersections_cb<ComponentsList>(this->pools, [this](entity_range found_range) {
				ranges.emplace_back(found_range);
				arguments.emplace_back(make_argument<Types...>(get_component<Types>(found_range.first(), this->pools)...));
			});
		});
	}

	template <typename... Ts>
	static auto make_argument(auto... args) {
		if constexpr (FirstIsEntity) {
			return [=](UpdateFn& fn, entity_id ent, entity_offset offset) { fn(ent, extract_arg_lambda<Ts>(args, offset, 0)...); };
		} else {
			return [=](UpdateFn& fn, entity_id    , entity_offset offset) { fn(     extract_arg_lambda<Ts>(args, offset, 0)...); };
		}
	}

private:
	// Get the type of lambda containing the arguments
	using argument = std::remove_cvref_t<decltype(
		for_all_types<ComponentsList>([]<typename... Types>() {
			return make_argument<Types...>(component_argument<Types>{}...);
		}
	))>;

	std::vector<entity_range> ranges;
	std::vector<argument> arguments;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_RANGED_H_

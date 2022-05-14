#ifndef POOL_RANGE_WALKER_H_
#define POOL_RANGE_WALKER_H_

#include "../entity_id.h"
#include "../entity_range.h"
#include "parent_id.h"
#include "system_defs.h"

namespace ecs::detail {

// Linearly walks one-or-more component pools
template <class Pools>
struct pool_range_walker {
	pool_range_walker(Pools const _pools) : pools(_pools) {}

	void reset(entity_range_view view) {
		ranges.assign(view.begin(), view.end());
		it = ranges.begin();
	}

	bool done() const {
		return it == ranges.end();
	}

	void next() {
		++it;
	}

	// Get the current range
	entity_range get_range() const {
		return *it;
	}

	// Get an entities component from a component pool
	template <typename Component>
	[[nodiscard]] auto get() const {
		using T = std::remove_cvref_t<Component>;

		entity_id const entity = it->first();

		// Filter: return a nullptr
		if constexpr (std::is_pointer_v<T>) {
			static_cast<void>(entity);
			return nullptr;

			// Tag: return a pointer to some dummy storage
		} else if constexpr (tagged<T>) {
			static char dummy_arr[sizeof(T)];
			return reinterpret_cast<T*>(dummy_arr);

			// Global: return the shared component
		} else if constexpr (global<T>) {
			return &get_pool<T>(pools).get_shared_component();

			// Parent component: return the parent with the types filled out
		} else if constexpr (std::is_same_v<reduce_parent_t<T>, parent_id>) {
			using parent_type = std::remove_cvref_t<Component>;
			parent_id pid = *get_pool<parent_id>(pools).find_component_data(entity);

			auto const tup_parent_ptrs = apply_type<parent_type_list_t<parent_type>>(
				[&]<typename... ParentTypes>() {
					return std::make_tuple(get_entity_data<std::remove_pointer_t<ParentTypes>>(pid, pools)...);
				});

			return parent_type{pid, tup_parent_ptrs};

			// Standard: return the component from the pool
		} else {
			return get_pool<T>(pools).find_component_data(entity);
		}
	}

private:
	std::vector<entity_range> ranges;
	std::vector<entity_range>::iterator it;
	Pools const pools;
};

} // namespace ecs::detail

#endif // !POOL_RANGE_WALKER_H_

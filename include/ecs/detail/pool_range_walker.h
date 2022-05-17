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
		return get_component<Component>(get_range().first(), pools);
	}

private:
	std::vector<entity_range> ranges;
	std::vector<entity_range>::iterator it;
	Pools const pools;
};

} // namespace ecs::detail

#endif // !POOL_RANGE_WALKER_H_

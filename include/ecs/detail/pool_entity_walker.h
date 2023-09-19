#ifndef POOL_ENTITY_WALKER_H_
#define POOL_ENTITY_WALKER_H_

#include "../entity_id.h"
#include "../entity_range.h"
#include "parent_id.h"
#include "system_defs.h"

namespace ecs::detail {

// The type of a single component argument
template <typename Component>
using walker_argument = reduce_parent_t<std::remove_cvref_t<Component>>*;

template <typename T>
struct pool_type_detect; // primary template

template <template <typename> typename Pool, typename Type>
struct pool_type_detect<Pool<Type>> {
	using type = Type;
};
template <template <typename> typename Pool, typename Type>
struct pool_type_detect<Pool<Type>*> {
	using type = Type;
};

template <typename T>
struct tuple_pool_type_detect; // primary template

template <template <typename...> typename Tuple, typename... PoolTypes> // partial specialization
struct tuple_pool_type_detect<const Tuple<PoolTypes* const...>> {
	using type = std::tuple<typename pool_type_detect<PoolTypes>::type*...>;
};

template <typename T>
using tuple_pool_type_detect_t = typename tuple_pool_type_detect<T>::type;

// Linearly walks one-or-more component pools
// TODO why is this not called an iterator?
template <typename Pools>
struct pool_entity_walker {
	void reset(Pools* _pools, entity_range_view view) {
		pools = _pools;
		ranges = view;
		ranges_it = ranges.begin();
		offset = 0;

		//update_pool_offsets();
	}

	bool done() const {
		return ranges_it == ranges.end();
	}

	void next_range() {
		++ranges_it;
		offset = 0;

		//if (!done())
		//	update_pool_offsets();
	}

	void next() {
		if (offset == static_cast<entity_type>(ranges_it->count()) - 1) {
			next_range();
		} else {
			++offset;
		}
	}

	// Get the current range
	entity_range get_range() const {
		Pre(!done());
		return *ranges_it;
	}

	// Get the current entity
	entity_id get_entity() const {
		Pre(!done());
		return ranges_it->first() + offset;
	}

	// Get an entities component from a component pool
	template <typename Component>
	[[nodiscard]] auto get() const {
		return get_component<Component>(get_entity(), *pools);
	}

private:
	// The ranges to iterate over
	entity_range_view ranges;

	// Iterator over the current range
	entity_range_view::iterator ranges_it;

	// Pointers to the start of each pools data
	//tuple_pool_type_detect_t<Pools> pointers;

	// Entity id and pool-pointers offset
	entity_type offset;

	// The tuple of pools in use
	Pools* pools;
};

} // namespace ecs::detail

#endif // !POOL_ENTITY_WALKER_H_

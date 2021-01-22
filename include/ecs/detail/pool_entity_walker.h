#ifndef POOL_ENTITY_WALKER_H_
#define POOL_ENTITY_WALKER_H_

#include "../entity_id.h"
#include "../entity_range.h"
#include "parent_id.h"
#include "system_defs.h"

namespace ecs::detail {

// The type of a single component argument
template <typename Component>
using walker_argument = reduce_parent_t<std::remove_cvref_t<Component>> *;


template <typename T>
struct pool_type_detect; // primary template

template <template <class> class Pool, class Type>
struct pool_type_detect<Pool<Type>> {
	using type = Type;
};
template <template <class> class Pool, class Type>
struct pool_type_detect<Pool<Type> *> {
	using type = Type;
};


template <typename T>
struct tuple_pool_type_detect;											   // primary template

template <template<class...> class Tuple, class... PoolTypes> // partial specialization
struct tuple_pool_type_detect<const Tuple<PoolTypes *const...>> {
	using type = std::tuple<typename pool_type_detect<PoolTypes>::type * ...>;
};

template <typename T>
using tuple_pool_type_detect_t = typename tuple_pool_type_detect<T>::type;

// Linearly walks one-or-more component pools
// TODO why is this not called an iterator?
template <class Pools, class... Components>
struct pool_entity_walker {
	pool_entity_walker(Pools pools) : pools(pools) {
		ranges_it = ranges.end();
	}

	void reset(entity_range_view view) {
		ranges.assign(view.begin(), view.end());
		ranges_it = ranges.begin();
		offset = 0;

		update_pool_offsets();
	}

	bool done() const {
		return ranges_it == ranges.end();
	}

	void next_range() {
		++ranges_it;
		offset = 0;

		update_pool_offsets();
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
		Expects(!done());
		return *ranges_it;
	}

	// Get the current entity
	entity_id get_entity() const {
		Expects(!done());
		return ranges_it->first() + offset;
	}

	// Get an entities component from a component pool
	template <typename Component>
	[[nodiscard]] auto get() const {
		using T = std::remove_cvref_t<Component>;

		if constexpr (std::is_pointer_v<T>) {
			// Filter: return a nullptr
			return nullptr;

		} else if constexpr (tagged<T>) {
			// Tag: return a pointer to some dummy storage
			thread_local char dummy_arr[sizeof(T)];
			return reinterpret_cast<T *>(dummy_arr);

		} else if constexpr (global<T>) {
			// Global: return the shared component
			return &get_pool<T>(pools).get_shared_component();

		} else if constexpr (std::is_same_v<reduce_parent_t<T>, parent_id>) {
			// Parent component: return the parent with the types filled out
			using parent_type = std::remove_cvref_t<Component>;
			parent_id pid = *(std::get<parent_id*>(pointers) + offset);

			parent_types_tuple_t<parent_type> pt;
			auto const tup_parent_ptrs = std::apply(
				[&](auto... parent_types) { return std::make_tuple(get_entity_data<decltype(parent_types)>(pid, pools)...); }, pt);

			return parent_type{pid, tup_parent_ptrs};
		} else {
			// Standard: return the component from the pool
			return (std::get<T *>(pointers) + offset);
		}
	}

private:
	void update_pool_offsets() {
		if (done())
			return;

		std::apply([this](auto *const... pools) {
				auto const f = [&](auto pool) {
					using pool_inner_type = typename pool_type_detect<decltype(pool)>::type;
					std::get<pool_inner_type *>(pointers) = pool->find_component_data(ranges_it->first());
				};

				(f(pools), ...);
			},
			pools);
	}

private:
	// The ranges to iterate over
	std::vector<entity_range> ranges;

	// Iterator over the current range
	std::vector<entity_range>::iterator ranges_it;

	// Pointers to the start of each pools data
	tuple_pool_type_detect_t<Pools> pointers;
	
	// Entity id and pool-pointers offset
	entity_type offset;

	// The tuple of pools in use
	Pools pools;
};

} // namespace ecs::detail

#endif // !POOL_ENTITY_WALKER_H_

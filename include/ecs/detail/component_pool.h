#ifndef ECS_COMPONENT_POOL
#define ECS_COMPONENT_POOL

#include <cstring> // for memcmp
#include <execution>
#include <functional>
#include <memory_resource>
#include <tuple>
#include <type_traits>
#include <vector>

#include "tls/collect.h"

#include "../entity_id.h"
#include "../entity_range.h"
#include "parent_id.h"

#include "component_pool_base.h"
#include "flags.h"
#include "options.h"

// Helpre macro for components that wish to support pmr.
// Declares the 'allocator_type' and default constructor/assignment
#define ECS_USE_PMR(ClassName)                                                                                                             \
	using allocator_type = std::pmr::polymorphic_allocator<>;                                                                              \
                                                                                                                                           \
	ClassName() : ClassName(allocator_type{}) {}                                                                                           \
	ClassName(ClassName const&) = default;                                                                                                 \
	ClassName(ClassName&&) = default;                                                                                                      \
	~ClassName() = default;                                                                                                                \
                                                                                                                                           \
	ClassName& operator=(ClassName const&) = default;                                                                                      \
	ClassName& operator=(ClassName&&) = default

namespace ecs::detail {

template <class ForwardIt, class BinaryPredicate>
constexpr ForwardIt std_combine_erase(ForwardIt first, ForwardIt last, BinaryPredicate p) {
	if (first == last)
		return last;

	ForwardIt result = first;
	while (++first != last) {
		auto const pred_res = p(*result, *first);
		if (!pred_res && ++result != first) {
			*result = std::move(*first);
		}
	}
	return ++result;
}

template <class Cont, class BinaryPredicate>
constexpr void combine_erase(Cont& cont, BinaryPredicate p) {
	auto const end = std_combine_erase(cont.begin(), cont.end(), p);
	cont.erase(end, cont.end());
}

template <typename T>
class component_pool final : public component_pool_base {
private:
	static_assert(!is_parent<T>::value, "can not have pools of any ecs::parent<type>");

	// The components
	//std::pmr::vector<T> components;
	std::vector<T> components;

	// The entities that have components in this storage.
	std::vector<entity_range> ranges;

	// The offset from a range into the components
	std::vector<ptrdiff_t> offsets;

	// Keep track of which components to add/remove each cycle
	using entity_data = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, T>>;
	using entity_init = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, std::function<const T(entity_id)>>>;
	tls::collect<std::vector<entity_data>, component_pool<T>> deferred_adds;
	tls::collect<std::vector<entity_init>, component_pool<T>> deferred_init_adds;
	tls::collect<std::vector<entity_range>, component_pool<T>> deferred_removes;

	// Status flags
	bool components_added = false;
	bool components_removed = false;
	bool components_modified = false;

public:
	constexpr component_pool() noexcept {};

	// Returns the current memory resource
	std::pmr::memory_resource* get_memory_resource() const {
#if 0
		return components.get_allocator().resource();
#else
		return nullptr;
#endif
	}

	// Sets the memory resource used to allocate components.
	// If components are already allocated, they will be moved.
	void set_memory_resource(std::pmr::memory_resource* /*resource*/) {
#if 0
		// Do nothing if the memory resource is already set
		if (components.get_allocator().resource() == resource)
			return;

		// Move the current data out
		auto copy{std::move(components)};

		// Destroy the current container
		std::destroy_at(&components);

		// Placement-new the data back with the new memory resource
		std::construct_at(&components, std::move(copy), resource);

		// component addresses has changed, so make sure systems rebuilds their caches
		components_added = true;
		components_removed = true;
		components_modified = true;
#endif
	}

	// Add a component to a range of entities, initialized by the supplied user function
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	template <typename Fn>
	constexpr void add_init(entity_range const range, Fn&& init) {
		// Add the range and function to a temp storage
		deferred_init_adds.local().emplace_back(range, std::forward<Fn>(init));
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	constexpr void add(entity_range const range, T&& component) {
		if constexpr (tagged<T>) {
			deferred_adds.local().push_back(range);
		} else {
			deferred_adds.local().emplace_back(range, std::forward<T>(component));
		}
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	constexpr void add(entity_range const range, T const& component) {
		if constexpr (tagged<T>) {
			deferred_adds.local().push_back(range);
		} else {
			deferred_adds.local().emplace_back(range, component);
		}
	}

	// Return the shared component
	T& get_shared_component() requires unbound<T> {
		static T t{};
		return t;
	}

	// Remove an entity from the component pool. This logically removes the component from the
	// entity.
	void remove(entity_id const id) {
		remove_range({id, id});
	}

	// Remove an entity from the component pool. This logically removes the component from the
	// entity.
	void remove_range(entity_range const range) {
		deferred_removes.local().push_back(range);
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	T* find_component_data(entity_id const id) {
		auto const index = find_entity_index(id);
		return index ? &components[static_cast<size_t>(index.value())] : nullptr;
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	T const* find_component_data(entity_id const id) const {
		auto const index = find_entity_index(id);
		return index ? &components[static_cast<size_t>(index.value())] : nullptr;
	}

	// Merge all the components queued for addition to the main storage,
	// and remove components queued for removal
	void process_changes() override {
		process_remove_components();
		process_add_components();
	}

	// Returns the number of active entities in the pool
	[[nodiscard]] constexpr
	size_t num_entities() const {
		return offsets.empty() ? 0 : static_cast<size_t>(offsets.back() + ranges.back().count());
	}

	// Returns the number of active components in the pool
	[[nodiscard]] constexpr
	size_t num_components() const {
		if constexpr (unbound<T>)
			return 1;
		else
			return components.size();
	}

	// Clears the pools state flags
	void clear_flags() override {
		components_added = false;
		components_removed = false;
		components_modified = false;
	}

	// Returns true if components has been added since last clear_flags() call
	[[nodiscard]] constexpr
	bool has_more_components() const {
		return components_added;
	}

	// Returns true if components has been removed since last clear_flags() call
	[[nodiscard]] constexpr
	bool has_less_components() const {
		return components_removed;
	}

	// Returns true if components has been added/removed since last clear_flags() call
	[[nodiscard]] constexpr
	bool has_component_count_changed() const {
		return components_added || components_removed;
	}

	[[nodiscard]] constexpr
	bool has_components_been_modified() const {
		return has_component_count_changed() || components_modified;
	}

	// Returns the pools entities
	entity_range_view get_entities() const {
		if constexpr (detail::global<T>) {
			// globals are accessible to all entities
			static constexpr entity_range global_range{std::numeric_limits<ecs::detail::entity_type>::min(),
													   std::numeric_limits<ecs::detail::entity_type>::max()};
			return entity_range_view{&global_range, 1};
		} else {
			return ranges;
		}
	}

	// Returns true if an entity has a component in this pool
	bool has_entity(entity_id const id) const {
		return has_entity({id, id});
	}

	// Returns true if an entity range has components in this pool
	bool has_entity(entity_range const& range) const {
		if (ranges.empty()) {
			return false;
		}

		auto const it = std::lower_bound(ranges.begin(), ranges.end(), range);
		if (it == ranges.end())
			return false;
		return it->contains(range);
	}

	// TODO remove?
	// Checks the current threads queue for the entity
	bool is_queued_add(entity_id const id) {
		return is_queued_add({id, id});
	}

	// Checks the current threads queue for the entity
	bool is_queued_add(entity_range const& range) {
		if (deferred_adds.local().empty()) {
			return false;
		}

		for (auto const& ents : deferred_adds.local()) {
			if (std::get<0>(ents).contains(range)) {
				return true;
			}
		}

		return false;
	}

	// Checks the current threads queue for the entity
	bool is_queued_remove(entity_id const id) {
		return is_queued_remove({id, id});
	}

	// Checks the current threads queue for the entity
	bool is_queued_remove(entity_range const& range) {
		if (deferred_removes.local().empty())
			return false;

		for (auto const& ents : deferred_removes.local()) {
			if (ents.contains(range))
				return true;
		}

		return false;
	}

	// Clear all entities from the pool
	void clear() override {
		// Remember if components was removed from the pool
		bool const is_removed = !components.empty();

		// Clear the pool
		ranges.clear();
		offsets.clear();
		components.clear();
		deferred_adds.reset();
		deferred_init_adds.reset();
		deferred_removes.reset();
		clear_flags();

		// Save the removal state
		components_removed = is_removed;
	}

	// Flag that components has been modified
	void notify_components_modified() {
		components_modified = true;
	}

private:
	// Flag that components has been added
	void set_data_added() {
		components_added = true;
	}

	// Flag that components has been removed
	void set_data_removed() {
		components_removed = true;
	}

	// Searches for an entitys offset in to the component pool.
	// Returns nothing if 'ent' is not a valid entity
	std::optional<ptrdiff_t> find_entity_index(entity_id const ent) const {
		if (ranges.empty() /*|| !has_entity(ent)*/) {
			return {};
		}

		auto const it = std::lower_bound(ranges.begin(), ranges.end(), ent);
		if (it == ranges.end() || !it->contains(ent))
			return {};

		auto const dist = std::distance(ranges.begin(), it);
		auto const range_offset = offsets.begin() + dist;
		return *range_offset + it->offset(ent);
	}

	// Add new queued entities and components to the main storage
	void process_add_components() {
		// Combine the components in to a single vector
		auto collection = deferred_adds.gather();
		std::vector<entity_data> adds;
		for (auto& vec : collection) {
			std::move(vec.begin(), vec.end(), std::back_inserter(adds));
		}

		auto collection_inits = deferred_init_adds.gather();
		std::vector<entity_init> inits;
		for (auto& vec : collection_inits) {
			std::move(vec.begin(), vec.end(), std::back_inserter(inits));
		}

		if (adds.empty() && inits.empty()) {
			return;
		}

		// Clear the current adds
		deferred_adds.reset();
		deferred_init_adds.reset();

		// Sort the input
		auto constexpr comparator = [](auto const& l, auto const& r) { return std::get<0>(l).first() < std::get<0>(r).first(); };
		std::sort(std::execution::par, adds.begin(), adds.end(), comparator);
		std::sort(std::execution::par, inits.begin(), inits.end(), comparator);

		// Check the 'add*' functions precondition.
		// An entity can not have more than one of the same component
		auto const has_duplicate_entities = [](auto const& vec) {
			return vec.end() != std::adjacent_find(vec.begin(), vec.end(),
												   [](auto const& l, auto const& r) { return std::get<0>(l) == std::get<0>(r); });
		};
		Expects(false == has_duplicate_entities(adds));

		// Merge adjacent ranges
		if constexpr (!detail::unbound<T>) { // contains data
			combine_erase(adds, [](entity_data& a, entity_data const& b) {
				auto& [a_rng, a_data] = a;
				auto const& [b_rng, b_data] = b;

				if (a_rng.adjacent(b_rng) && 0 == memcmp(&a_data, &b_data, sizeof(T))) {
					a_rng = entity_range::merge(a_rng, b_rng);
					return true;
				} else {
					return false;
				}
			});
			combine_erase(inits, [](entity_init& a, entity_init const& b) {
				auto a_rng = std::get<0>(a);
				auto const b_rng = std::get<0>(b);

#if defined(__clang__) && defined(_MSVC_STL_VERSION)
				// Clang with ms-stl fails if these are const
				auto a_func = std::get<1>(a);
				auto b_func = std::get<1>(b);
#else
                    auto const a_func = std::get<1>(a);
                    auto const b_func = std::get<1>(b);
#endif

				if (a_rng.adjacent(b_rng) && (a_func.template target<T(entity_id)>() == b_func.template target<T(entity_id)>())) {
					a_rng = entity_range::merge(a_rng, b_rng);
					return true;
				} else {
					return false;
				}
			});
		} else {											   // does not contain data
			auto const combiner = [](auto& a, auto const& b) { // entity_data/entity_init
				auto& [a_rng] = a;
				auto const& [b_rng] = b;

				if (a_rng.adjacent(b_rng)) {
					a_rng = entity_range::merge(a_rng, b_rng);
					return true;
				} else {
					return false;
				}
			};
			combine_erase(adds, combiner);
			combine_erase(inits, combiner);
		}

		// Add the new entities/components
		std::vector<entity_range> new_ranges;
		auto it_adds = adds.begin();
		auto ranges_it = ranges.cbegin();

		auto const insert_range = [&](auto const it) {
			entity_range const& range = std::get<0>(*it);
			ptrdiff_t offset = 0;

			// Copy the current ranges while looking for an insertion point
			while (ranges_it != ranges.cend() && (*ranges_it < range)) {
				if constexpr (!unbound<T>) {
					// Advance the component offset so it will point
					// to the correct components when inserting
					offset += ranges_it->count();
				}

				new_ranges.push_back(*ranges_it++);
			}

			// New range must not already exist in the pool
			if (ranges_it != ranges.cend())
				Expects(false == ranges_it->overlaps(range));

			// Add or merge the new range
			if (!new_ranges.empty() && new_ranges.back().adjacent(range)) {
				// Merge the new range with the last one in the vector
				new_ranges.back() = ecs::entity_range::merge(new_ranges.back(), range);
			} else {
				// Add the new range
				new_ranges.push_back(range);
			}

			// return the offset
			return offset;
		};

		if constexpr (!detail::unbound<T>) {
			auto it_inits = inits.begin();
			auto component_it = components.cbegin();

			auto const insert_data = [&](ptrdiff_t offset) {
				// Add the new data
				std::advance(component_it, offset);
				ptrdiff_t const range_count = std::get<0>(*it_adds).count();
				component_it = components.insert(component_it, static_cast<size_t>(range_count), std::move(std::get<1>(*it_adds)));
				std::advance(component_it, range_count);
			};
			auto const insert_init = [&](ptrdiff_t offset) {
				// Add the new data
				std::advance(component_it, offset);
				auto const& range = std::get<0>(*it_inits);
				auto const& init = std::get<1>(*it_inits);
				for (entity_id const ent : range) {
					component_it = components.emplace(component_it, init(ent));
					std::advance(component_it, 1);
				}
			};

			while (it_adds != adds.end() && it_inits != inits.end()) {
				if (std::get<0>(*it_adds) < std::get<0>(*it_inits)) {
					insert_data(insert_range(it_adds));
					++it_adds;
				} else {
					insert_init(insert_range(it_inits));
					++it_inits;
				}
			}

			while (it_adds != adds.end()) {
				insert_data(insert_range(it_adds));
				++it_adds;
			}
			while (it_inits != inits.end()) {
				insert_init(insert_range(it_inits));
				++it_inits;
			}
		} else {
			// If there is no data, the ranges are always added to 'deferred_adds'
			while (it_adds != adds.end()) {
				insert_range(it_adds);
				++it_adds;
			}
		}

		// Move the remaining ranges
		std::move(ranges_it, ranges.cend(), std::back_inserter(new_ranges));

		// Store the new ranges
		ranges = std::move(new_ranges);

		// Calculate offsets
		offsets.clear();
		std::exclusive_scan(ranges.begin(), ranges.end(), std::back_inserter(offsets), ptrdiff_t{0},
							[](ptrdiff_t init, entity_range range) { return init + range.count(); });

		// Update the state
		set_data_added();
	}

	// Removes the entities and components
	void process_remove_components() {
		// Transient components are removed each cycle
		if constexpr (detail::transient<T>) {
			if (!ranges.empty()) {
				ranges.clear();
				components.clear();
				set_data_removed();
			}
		} else {
			// Combine the vectors
			auto collection = deferred_removes.gather();
			std::vector<entity_range> removes;
			for (auto& vec : collection) {
				std::move(vec.begin(), vec.end(), std::back_inserter(removes));
			}

			if (removes.empty()) {
				return;
			}

			// Clear the current removes
			deferred_removes.reset();

			// Sort it if needed
			if (!std::is_sorted(removes.begin(), removes.end()))
				std::sort(removes.begin(), removes.end());

			// An entity can not have more than one of the same component
			auto const has_duplicate_entities = [](auto const& vec) { return vec.end() != std::adjacent_find(vec.begin(), vec.end()); };
			Expects(false == has_duplicate_entities(removes));

			// Merge adjacent ranges
			auto const combiner = [](auto& a, auto const& b) {
				if (a.adjacent(b)) {
					a = entity_range::merge(a, b);
					return true;
				} else {
					return false;
				}
			};
			combine_erase(removes, combiner);

			// Remove the components
			if constexpr (!unbound<T>) {
				// Find the first valid index
				auto index = find_entity_index(removes.front().first());
				Expects(index.has_value());
				auto dest_it = components.begin() + static_cast<ptrdiff_t>(index.value());
				auto from_it = dest_it + static_cast<ptrdiff_t>(removes.front().count());

				if (dest_it == components.begin() && from_it == components.end()) {
					components.clear();
				} else {
					// Move components inbetween the ranges
					for (auto it = removes.cbegin() + 1; it != removes.cend(); ++it) {
						index = find_entity_index(it->first());

						auto const last_it = components.begin() + index.value();
						auto const dist = std::distance(from_it, last_it);
						from_it = std::move(from_it, last_it, dest_it);
						dest_it += dist;
					}

					// Move rest of components
					auto const dist = std::distance(from_it, components.end());
					std::move(from_it, components.end(), dest_it);

					// Erase the unused space
					if (dest_it + dist != components.end()) {
						components.erase(dest_it + dist, components.end());
					} else {
						components.erase(dest_it, components.end());
					}
				}
			}

			// Remove the ranges
			auto curr_range = ranges.begin();
			for (auto const& remove : removes) {
				// Step forward until a candidate range is found
				while (*curr_range < remove && curr_range != ranges.end()) {
					++curr_range;
				}

				if (curr_range == ranges.end()) {
					break;
				}

				Expects(curr_range->contains(remove));

				// Erase the current range if it equals the range to be removed
				if (curr_range->equals(remove)) {
					curr_range = ranges.erase(curr_range);
				} else {
					// Do the removal
					auto result = entity_range::remove(*curr_range, remove);

					// Update the modified range
					*curr_range = result.first;

					// If the range was split, add the other part of the range
					if (result.second.has_value()) {
						curr_range = ranges.insert(curr_range + 1, result.second.value());
					}
				}
			}

			// Calculate offsets
			offsets.clear();
			std::exclusive_scan(ranges.begin(), ranges.end(), std::back_inserter(offsets), ptrdiff_t{0},
								[](ptrdiff_t init, entity_range range) { return init + range.count(); });

			// Update the state
			set_data_removed();
		}
	}
};
} // namespace ecs::detail

#endif // !ECS_COMPONENT_POOL

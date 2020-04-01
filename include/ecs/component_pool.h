#ifndef __COMPONENT_POOL
#define __COMPONENT_POOL

#include <concepts>
#include <vector>
#include <functional>
#include <variant>
#include <numeric>
#include <type_traits>
#include <tuple>
#include "../threaded/threaded/threaded.h"
#include "component_specifier.h"
#include "component_pool_base.h"
#include "entity_id.h"
#include "entity_range.h"

namespace ecs::detail {
	// For std::visit
	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

	template <std::copyable T>
	class component_pool final : public component_pool_base {
	private:
		// The components
		std::vector<T> components;

		// The entities that have components in this storage.
		std::vector<entity_range> ranges;

		// Keep track of which components to add/remove each cycle
		using variant = std::variant<T, std::function<T(entity_id)>>;
		using entity_data = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, variant>>;
		threaded<std::vector<entity_data>> deferred_adds;
		threaded<std::vector<entity_range>> deferred_removes;

		// Status flags
		bool data_added = false;
		bool data_removed = false;

	public:
		// Add a component to a range of entities, initialized by the supplied user function
		// Pre: entities has not already been added, or is in queue to be added
		//      This condition will not be checked until 'process_changes' is called.
		template <typename Fn> requires (!unbound<T>)
		void add_init(entity_range const range, Fn&& init) {
			// Add the range and function to a temp storage
			deferred_adds.local().emplace_back(range, std::forward<Fn>(init));
		}

		// Add a shared/tagged component to a range of entity.
		// Pre: entities has not already been added, or is in queue to be added
		//      This condition will not be checked until 'process_changes' is called.
		void add(entity_range const range, T&& /* unused */) requires unbound<T> {
			deferred_adds.local().push_back(range);
		}

		// Add a component to a range of entity.
		// Pre: entities has not already been added, or is in queue to be added
		//      This condition will not be checked until 'process_changes' is called.
		void add(entity_range const range, T&& component) {
			deferred_adds.local().emplace_back(range, std::forward<T>(component));
		}

		// Add a component to an entity.
		// Pre: entity has not already been added, or is in queue to be added.
		//      This condition will not be checked until 'process_changes' is called.
		void add(entity_id const id, T&& component) {
			add({ id, id }, std::forward<T>(component));
		}

		// Return the shared component
		T& get_shared_component() requires unbound<T> {
			static T t;
			return t;
		}

		// Remove an entity from the component pool. This logically removes the component from the entity.
		void remove(entity_id const id) {
			remove_range({ id, id });
		}

		// Remove an entity from the component pool. This logically removes the component from the entity.
		void remove_range(entity_range const range) {
			if (!has_entity(range)) {
				return;
			}

			auto& rem = deferred_removes.local();
			if (!rem.empty() && rem.back().can_merge(range)) {
				rem.back() = entity_range::merge(rem.back(), range);
			}
			else {
				rem.push_back(range);
			}
		}

		// Returns the shared component
		T* find_component_data(entity_id const /*id*/) requires unbound<T> {
			return &get_shared_component();
		}

		// Returns an entities component.
		// Returns nullptr if the entity is not found in this pool
		T* find_component_data(entity_id const id) {
			auto const index = find_entity_index(id);
			return index ? &components[index.value()] : nullptr;
		}

		// Merge all the components queued for addition to the main storage,
		// and remove components queued for removal
		void process_changes() override {
			process_remove_components();
			process_add_components();
		}

		// Returns the number of active entities in the pool
		size_t num_entities() const {
			return std::accumulate(ranges.begin(), ranges.end(), size_t{ 0 }, [](size_t val, entity_range const& range) { return val + range.count(); });
		}

		// Returns the number of active components in the pool
		size_t num_components() const {
			if constexpr (unbound<T>)
				return 1;
			else
				return components.size();
		}

		// Clears the pools state flags
		void clear_flags() override {
			data_added = false;
			data_removed = false;
		}

		// Returns true if components has been added since last clear_flags() call
		bool is_data_added() const {
			return data_added;
		}

		// Returns true if components has been removed since last clear_flags() call
		bool is_data_removed() const {
			return data_removed;
		}

		// Returns true if components has been added/removed since last clear_flags() call
		bool is_data_modified() const {
			return data_added || data_removed;
		}

		// Returns the pools entities
		entity_range_view get_entities() const {
			return ranges;
		}

		// Returns true if an entity has a component in this pool
		bool has_entity(entity_id const id) const {
			return has_entity({ id, id });
		}

		// Returns true if an entity range has components in this pool
		bool has_entity(entity_range const& range) const {
			if (ranges.empty()) {
				return false;
			}

			for (entity_range const& r : ranges) {
				if (r.contains(range)) {
					return true;
				}
			}

			return false;
		}

		// Checks the current threads queue for the entity
		bool is_queued_add(entity_id const id) {
			return is_queued_add({ id, id });
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
			return is_queued_remove({ id, id });
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
			components.clear();
			deferred_adds.clear();
			deferred_removes.clear();
			clear_flags();

			// Save the removal state
			data_removed = is_removed;
		}

	private:
		// Flag that components has been added
		void set_data_added() {
			data_added = true;
		}

		// Flag that components has been removed
		void set_data_removed() {
			data_removed = true;
		}

		// Searches for an entitys offset in to the component pool.
		// Returns nothing if 'ent' is not a valid entity
		std::optional<size_t> find_entity_index(entity_id const ent) const {
			if (ranges.empty() || !has_entity(ent)) {
				return {};
			}

			// Run through the ranges
			size_t index = 0;
			for (entity_range const& range : ranges) {
				if (!range.contains(ent)) {
					index += range.count();
					continue;
				}

				index += range.offset(ent);
				return index;
			}

			return {};
		}

		// Add new queued entities and components to the main storage
		void process_add_components() {
			// Combine the components in to a single vector
			std::vector<entity_data> adds;
			for (auto& vec : deferred_adds) {
				std::move(vec.begin(), vec.end(), std::back_inserter(adds));
			}

			if (adds.empty()) {
				return;
			}

			// Clear the current adds
			deferred_adds.clear();

			// Sort the input
			auto constexpr comparator = [](entity_data const& l, entity_data const& r) {
				return std::get<0>(l).first() < std::get<0>(r).first();
			};
			if (!std::is_sorted(adds.begin(), adds.end(), comparator)) {
				std::sort(adds.begin(), adds.end(), comparator);
			}

			// Check the 'add*' functions precondition.
			// An entity can not have more than one of the same component
			auto const has_duplicate_entities = [](auto const& vec) {
				return vec.end() != std::adjacent_find(vec.begin(), vec.end(), [](auto const& l, auto const& r) {
					return std::get<0>(l) == std::get<0>(r);
				});
			};
			Expects(false == has_duplicate_entities(adds));

			// Small helper function for combining ranges
			auto const add_range = [](std::vector<entity_range>& dest, entity_range const& range) {
				// Merge the range or add it
				if (!dest.empty() && dest.back().can_merge(range)) {
					dest.back() = entity_range::merge(dest.back(), range);
				}
				else {
					dest.push_back(range);
				}
			};

			// Add the new entities/components
			std::vector<entity_range> new_ranges;
			auto ranges_it = ranges.cbegin();
			[[maybe_unused]] auto component_it = components.cbegin();
			for (auto& add : adds) {
				entity_range const& range = std::get<0>(add);

				// Copy the current ranges while looking for an insertion point
				while (ranges_it != ranges.cend() && (*ranges_it < range)) {
					if constexpr (!unbound<T>) {
						// Advance the component iterator so it will point to the correct components when i start inserting it
						component_it += ranges_it->count();
					}

					add_range(new_ranges, *ranges_it++);
				}

				// New range must not already exist in the pool
				if (ranges_it != ranges.cend())
					Expects(false == ranges_it->overlaps(range));

				// Add the new range
				add_range(new_ranges, range);

				if constexpr (!unbound<T>) {
					auto const add_val = [this, &component_it, range](T&& val) {
						component_it = components.insert(component_it, range.count(), std::forward<T>(val));
						component_it = std::next(component_it, range.count());
					};
					auto const add_init = [this, &component_it, range](std::function<T(entity_id)> init) {
						for (entity_id ent = range.first(); ent <= range.last(); ++ent) {
							component_it = components.emplace(component_it, init(ent));
							component_it = std::next(component_it);
						}
					};

					// Add the new components
					std::visit(overloaded{ add_val, add_init }, std::move(std::get<1>(add)));
				}
			}

			// Move the remaining ranges
			std::move(ranges_it, ranges.cend(), std::back_inserter(new_ranges));

			// Store the new ranges
			ranges = std::move(new_ranges);

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
			}
			else {
				// Combine the vectors
				std::vector<entity_range> removes;
				for (auto& vec : deferred_removes) {
					std::move(vec.begin(), vec.end(), std::back_inserter(removes));
				}

				if (removes.empty()) {
					return;
				}

				// Clear the current removes
				deferred_removes.clear();

				// Sort it if needed
				if (!std::is_sorted(removes.begin(), removes.end())) {
					std::sort(removes.begin(), removes.end());
				}

				// An entity can not have more than one of the same component
				auto const has_duplicate_entities = [](auto const& vec) {
					return vec.end() != std::adjacent_find(vec.begin(), vec.end());
				};
				Expects(false == has_duplicate_entities(removes));

				// Remove the components
				if constexpr (!unbound<T>) {
					// Find the first valid index
					auto index = find_entity_index(removes.front().first());
					Expects(index.has_value());
					auto dest_it = components.begin() + index.value();
					auto from_it = dest_it + removes.front().count();

					if (dest_it == components.begin() && from_it == components.end()) {
						components.clear();
					}
					else {
						// Move components between the ranges
						for (auto it = removes.cbegin() + 1; it != removes.cend(); ++it) {
							index = find_entity_index(it->first());
							Expects(index.has_value());

							auto const last_it = components.begin() + index.value();
							auto const dist = std::distance(from_it, last_it);
							from_it = std::move(from_it, last_it, dest_it);
							dest_it += dist;
						}

						// Move rest of components
						auto const dist = std::distance(from_it, components.end());
						std::move(from_it, components.end(), dest_it);
						dest_it += dist;

						// Erase the unused space
						components.erase(dest_it, components.end());
					}
				}

				// Remove the ranges
				auto curr_range = ranges.begin();
				for (auto const& remove : removes) {
					// Step forward until a candidate range is found
					while (!curr_range->contains(remove) && curr_range != ranges.end()) {
						++curr_range;
					}

					if (curr_range == ranges.end()) {
						break;
					}

					// Erase the current range if it equals the range to be removed
					if (curr_range->equals(remove)) {
						curr_range = ranges.erase(curr_range);
					}
					else {
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

				// Update the state
				set_data_removed();
			}
		}
	};
}

#endif // !__COMPONENT_POOL

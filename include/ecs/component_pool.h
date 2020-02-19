#pragma once
#include <gsl/gsl>
#include <gsl/span>
#include <variant>
#include <utility>
#include <cassert>

#include "../threaded/threaded/threaded.h"
#include "component_pool_base.h"
#include "component_specifier.h"
#include "entity_range.h"


namespace ecs::detail {
	// For std::visit
	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

	template <typename T>
	class component_pool final : public component_pool_base {

		static constexpr bool is_static_component = shared<T> || tagged<T>; // all entities point to the same component

	public:
		// Adds a component to an entity.
		// Pre: entity has not already been added, or is in queue to be added.
		//      This condition will not be checked until 'process_changes' is called.
		void add(entity_id const id, T component) {
			add({ id, id }, std::move(component));
		}

		// Adds a component to a range of entities, initialized by the supplied user function
		// Pre: entities has not already been added, or is in queue to be added
		//      This condition will not be checked until 'process_changes' is called.
		template <typename Fn>
			requires std::is_invocable_r_v<T, Fn, entity_id>
		void add_init(entity_range const range, Fn init) {
			Expects(!has_entity(range));
			Expects(!is_queued_add(range));

			if constexpr (is_static_component) {
				// Shared/tagged components will all point to the same instance, so only allocate room for 1 component
				if (data.size() == 0) {
					data.emplace_back(init(0));
				}

				deferred_adds.local().emplace_back(range);
			}
			else {
				// Add the id and data to a temp storage
				deferred_adds.local().emplace_back(range, std::function<T(ecs::entity_id)>{ init });
			}
		}

		// Adds a component to a range of entity.
		// Pre: entities has not already been added, or is in queue to be added
		//      This condition will not be checked until 'process_changes' is called.
		void add(entity_range const range, T component) {
			Expects(!has_entity(range));
			Expects(!is_queued_add(range));

			if constexpr (is_static_component) {
				// Shared/tagged components will all point to the same instance, so only allocate room for 1 component
				if (data.size() == 0) {
					data.emplace_back(std::move(component));
				}

				// Merge the range or add it
				if (deferred_adds.local().size() > 0 && deferred_adds.local().back().can_merge(range)) {
					deferred_adds.local().back() = entity_range::merge(deferred_adds.local().back(), range);
				}
				else {
					deferred_adds.local().push_back(range);
				}
			}
			else {
				// Try and merge the range instead of adding it
				if (deferred_adds.local().size() > 0) {
					auto& [last_range, val] = deferred_adds.local().back();
					if (last_range.can_merge(range)) {
						bool const equal_vals = 0 == std::memcmp(&std::get<T>(val), &component, sizeof(T));
						if (equal_vals) {
							last_range = entity_range::merge(last_range, range);
							return;
						}
					}
				}

				// Merge wasn't possible, so just add it
				deferred_adds.local().emplace_back(range, std::move(component));
			}
		}

		// Returns the shared component
		template <typename XT = T>
			requires shared<XT>
		T& get_shared_component() const {
			if (data.size() == 0) {
				data.emplace_back(T{});
			}

			return *data.data();
		}

		// Remove an entity from the component pool. This logically removes the component from the entity.
		void remove(entity_id const id) {
			remove_range({ id, id });
		}

		// Remove an entity from the component pool. This logically removes the component from the entity.
		void remove_range(entity_range const range) {
			if (!has_entity(range))
				return;

			auto& rem = deferred_removes.local();
			if (rem.size() > 0 && rem.back().can_merge(range)) {
				rem.back() = entity_range::merge(rem.back(), range);
			}
			else {
				rem.push_back(range);
			}
		}

		// Returns an entities component
		// Returns nullptr if the entity is not found in this pool
		T* find_component_data(entity_id const id) const {
			if constexpr (tagged<T>) {
				static T t;
				return &t;
			}
			else if constexpr (shared<T>) {
				// All entities point to the same component
				(void)id;
				return &get_shared_component<T>();
			}
			else {
				auto const index = find_entity_index(id);
				return index ? &data[index.value()] : nullptr;
			}
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
			return data.size();
		}

		// Clears the pools state flags
		void clear_flags() override {
			data_added = false;
			data_removed = false;
		}

		// Returns true if data has been added since last clear_flags() call
		bool is_data_added() const {
			return data_added;
		}

		// Returns true if data has been removed since last clear_flags() call
		bool is_data_removed() const {
			return data_removed;
		}

		// Returns true if data has been added/removed since last clear_flags() call
		bool is_data_modified() const {
			return data_added || data_removed;
		}

		// Returns the pools entities
		entity_range_view get_entities() const {
			return ranges;
		}

		// Returns true if an entity has data in this pool
		bool has_entity(entity_id const id) const {
			return has_entity({ id, id });
		}

		// Returns true if an entity range has data in this pool
		bool has_entity(entity_range const& range) const {
			if (ranges.empty())
				return false;

			for (entity_range const& r : ranges) {
				if (r.contains(range))
					return true;
			}

			return false;
		}

		// Checks the current threads queue for the entity
		bool is_queued_add(entity_id const id) {
			return is_queued_add({ id, id });
		}

		// Checks the current threads queue for the entity
		bool is_queued_add(entity_range const& range) {
			if (deferred_adds.local().empty())
				return false;

			if constexpr (is_static_component) {
				for (auto const& ents : deferred_adds.local()) {
					if (ents.contains(range))
						return true;
				}
			}
			else {
				for (auto const& ents : deferred_adds.local()) {
					if (ents.first.contains(range))
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
			// Remember is data was removed from the pool
			bool const is_removed = data.size() > 0;

			// Clear the pool
			ranges.clear();
			data.clear();
			deferred_adds.clear();
			deferred_removes.clear();
			clear_flags();

			// Save the removal state
			data_removed = is_removed;
		}

	private:
		// Flag that data has been added
		void set_data_added() {
			data_added = true;
		}

		// Flag that data has been removed
		void set_data_removed() {
			data_removed = true;
		}

		// Returns the component at the specific index.
		// Pre: index is within bounds
		T* at(size_t const index) const {
			if (/*index >= 0 &&*/ index < data.size())
				return &data[index];
			else
				return nullptr;
		}

		// Searches for an entitys offset in to the component pool.
		// Returns nothing if 'ent' is not a valid entity
		std::optional<size_t> find_entity_index(entity_id const ent) const {
			if (ranges.size() == 0 || !has_entity(ent))
				return {};

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

		// Add new queued entities and component data to the main storage
		void process_add_components() {
			// Combine the data in to a single vector
			std::vector<entity_data> adds;
			for (auto& vec : deferred_adds)
				std::move(vec.begin(), vec.end(), std::back_inserter(adds));

			if (adds.empty())
				return;

			// Check the 'add*' functions precondition.
			// An entity can not have more than one of the same component
			auto const has_duplicate_entities = [](auto const& vec) {
				return vec.end() != std::adjacent_find(vec.begin(), vec.end(), [](auto const& l, auto const& r) {
					if constexpr (is_static_component)
						return l == r;
					else
						return l.first == r.first;
				});
			};
			Expects(false == has_duplicate_entities(adds));

			// Clear the current adds
			deferred_adds.clear();

			// Sort the input
			auto constexpr comparator = [](entity_data const& l, entity_data const& r) {
				if constexpr (is_static_component)
					return l.first() < r.first();
				else
					return l.first.first() < r.first.first();
			};
			if (!std::is_sorted(adds.begin(), adds.end(), comparator))
				std::sort(adds.begin(), adds.end(), comparator);

			// Small helper function for combining ranges
			auto const add_range = [](std::vector<entity_range>& dest, entity_range const& range) {
				// Merge the range or add it
				if (dest.size() > 0 && dest.back().can_merge(range)) {
					dest.back() = entity_range::merge(dest.back(), range);
				}
				else {
					dest.push_back(range);
				}
			};

			// Add the new components
			if constexpr (is_static_component) {
				if (ranges.empty()) {
					ranges = std::move(adds);
				}
				else {
					// Do the inserts
					std::vector<entity_range> new_ranges;
					auto ranges_it = ranges.cbegin();
					for (entity_range const& range : adds) {
						// Copy the current ranges while looking for an insertion point
						while (ranges_it != ranges.cend() && (*ranges_it < range)) {
							new_ranges.push_back(*ranges_it++);
						}

						// Add the new range
						add_range(new_ranges, range);
					}

					// Move the remaining ranges
					std::move(ranges_it, ranges.cend(), std::back_inserter(new_ranges));

					// Store the new ranges
					ranges = std::move(new_ranges);
				}
			}
			else {
				std::vector<entity_range> new_ranges;
				auto ranges_it = ranges.cbegin();
				auto component_it = data.cbegin();
				for (auto const& [range, component_val] : adds) {
					// Copy the current ranges while looking for an insertion point
					while (ranges_it != ranges.cend() && (*ranges_it < range)) {
						// Advance the component iterator so it will point to the correct data when i start inserting it
						component_it += ranges_it->count();

						add_range(new_ranges, *ranges_it++);
					}

					// Add the new range
					add_range(new_ranges, range);

					// Add the new components
					auto const add_val = [this, &component_it, range = range](T const& val) {	// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0588r1.html
						component_it = data.insert(component_it, range.count(), val);
						component_it = std::next(component_it, range.count());
					};
					auto const add_init = [this, &component_it, range = range](std::function<T(entity_id)> init) {
						for (entity_id ent = range.first(); ent <= range.last(); ++ent.id) {
							component_it = data.insert(component_it, init(ent));
							component_it = std::next(component_it, 1);
						}
					};
					std::visit(detail::overloaded{ add_val, add_init }, component_val);
				}

				// Move the remaining ranges
				std::move(ranges_it, ranges.cend(), std::back_inserter(new_ranges));

				// Store the new ranges
				ranges = std::move(new_ranges);
			}

			// Update the state
			set_data_added();
		}

		// Removes the entities
		void process_remove_components() {
			// Transient components are removed each cycle
			if constexpr (transient<T>) {
				if (ranges.size() > 0) {
					ranges.clear();
					data.clear();
					set_data_removed();
				}
			}
			else {
				// Combine the vectors
				std::vector<entity_range> removes;
				for (auto& vec : deferred_removes)
					std::move(vec.begin(), vec.end(), std::back_inserter(removes));

				if (removes.empty())
					return;

				// Clear the current removes
				deferred_removes.clear();

				// An entity can not have more than one of the same component
				auto const has_duplicate_entities = [](auto const& vec) {
					return vec.end() != std::adjacent_find(vec.begin(), vec.end());
				};
				Expects(false == has_duplicate_entities(removes));

				// Sort it if needed
				if (!std::is_sorted(removes.begin(), removes.end()))
					std::sort(removes.begin(), removes.end());

				// Erase the ranges data
				if constexpr (!is_static_component) {
					// Find the first valid index
					auto index = find_entity_index(removes.front().first());
					Expects(index.has_value());
					auto dest_it = data.begin() + index.value();
					auto from_it = dest_it + removes.front().count();

					if (dest_it == data.begin() && from_it == data.end()) {
						data.clear();
					}
					else {
						// Move data between the ranges
						for (auto it = removes.cbegin() + 1; it != removes.cend(); ++it) {
							index = find_entity_index(it->first());
							assert(index.has_value());

							auto const last_it = data.begin() + index.value();
							auto const dist = std::distance(from_it, last_it);
							from_it = std::move(from_it, last_it, dest_it);
							dest_it += dist;
						}

						// Move rest of data
						auto const dist = std::distance(from_it, data.end());
						std::move(from_it, data.end(), dest_it);
						dest_it += dist;

						// Erase the unused space
						data.erase(dest_it, data.end());
					}
				}

				// Remove the ranges
				auto curr_range = ranges.begin();
				for (auto it = removes.begin(); it != removes.end(); ++it) {
					if (curr_range == ranges.end())
						break;

					// Step forward until a candidate range is found
					while (!curr_range->contains(*it))
						++curr_range;

					// Erase the current range if it equals the range to be removed
					if (curr_range->equals(*it)) {
						curr_range = ranges.erase(curr_range);
					}
					else {
						// Do the removal
						auto result = entity_range::remove(*curr_range, *it);

						// Update the modified range
						*curr_range = result.first;

						// If the range was split, add the other part of the range
						if (result.second.has_value())
							curr_range = ranges.insert(curr_range + 1, result.second.value());
					}
				}

				// Update the state
				set_data_removed();
			}
		}

	private:
		// The components data
		mutable std::vector<T> data; // mutable so the constants getters can return components whos contents can be modified

		// The entities that have data in this storage.
		std::vector<entity_range> ranges;


		// The type used to store data.
		using component_val = std::variant<T, std::function<T(entity_id)>>;
		using entity_data = std::conditional_t<is_static_component, entity_range, std::pair<entity_range, component_val>>;

		// Keep track of which components to add/remove each cycle
		threaded<std::vector<entity_data>> deferred_adds;
		threaded<std::vector<entity_range>> deferred_removes;

		// 
		bool data_added{};
		bool data_removed{};
	};
};

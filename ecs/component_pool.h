#pragma once
#include <gsl/gsl>
#include <gsl/span>
#include <variant>
#include <utility>

#include "../threaded/threaded/threaded.h"
#include "component_pool_base.h"
#include "component_specifier.h"
#include "entity_range.h"

namespace ecs::detail
{
	// True if each entity has their own unique component (ie. not shared across components)
	template <class T> constexpr bool has_unique_component_v = !(is_shared_v<T> || is_tagged_v<T>);

	// For std::visit
	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;


	enum modified_state : unsigned {
		none = 0,			// no change
		add = 1 << 0,		// entity/data was added
		remove = 1 << 1,	// entity/data was removed
	};

	template <typename T>
	class component_pool final : public component_pool_base
	{
	public:
		// Adds a component to an entity.
		// Pre: entity has not already been added, or is in queue to be added
		void add(entity_id const id, T component)
		{
			add_range({ id, id }, std::move(component));
		}

		// Adds a component to a range of entities, initialized by the supplied use function
		// Pre: entities has not already been added, or is in queue to be added
		template <typename Fn>
		void add_range_init(entity_range const range, Fn init)
		{
			static_assert(!(is_tagged_v<T> && sizeof(T) > 1), "Tagged components can not have any data in them");
			Expects(!has_entity_range(range));
			Expects(!is_queued_add_range(range));

			if constexpr (is_shared_v<T> || is_tagged_v<T>) {
				// Shared/tagged components will all point to the same instance, so only allocate room for 1 component
				if (data.size() == 0) {
					data.emplace_back(init(0));
				}

				deferred_adds->emplace_back(range);
			}
			else {
				// Add the id and data to a temp storage
				deferred_adds->emplace_back(range, std::function<T(ecs::entity_id)>{ init });
			}
		}

		// Adds a component to a range of entity.
		// Pre: entities has not already been added, or is in queue to be added
		void add_range(entity_range const range, T component)
		{
			static_assert(!(is_tagged_v<T> && sizeof(T) > 1), "Tagged components can not have any data in them");
			Expects(!has_entity_range(range));
			Expects(!is_queued_add_range(range));

			if constexpr (is_shared_v<T> || is_tagged_v<T>) {
				// Shared/tagged components will all point to the same instance, so only allocate room for 1 component
				if (data.size() == 0) {
					data.emplace_back(std::move(component));
				}

				// Merge the range or add it
				if (deferred_adds->size() > 0 && deferred_adds->back().can_merge(range)) {
					deferred_adds->back() = entity_range::merge(deferred_adds->back(), range);
				}
				else {
					deferred_adds->push_back(range);
				}
			}
			else {
				// Try and merge the range instead of adding it
				if (deferred_adds->size() > 0) {
					auto &[last_range, val] = deferred_adds->back();
					if (last_range.can_merge(range)) {
						bool const equal_vals = 0 == std::memcmp(&std::get<T>(val), &component, sizeof(T));
						if (equal_vals) {
							last_range = entity_range::merge(last_range, range);
							return;
						}
					}
				}

				// Merge wasn't possible, so just add it
				deferred_adds->emplace_back(range, std::move(component));
			}
		}

		// Returns the shared component
		template <typename = std::enable_if_t<is_shared_v<T>>>
		T* get_shared_component() const
		{
			if (data.size() == 0) {
				data.emplace_back(T{});
			}

			return at(0);
		}

		// Remove an entity from the component pool. This logically removes the component from the entity.
		void remove(entity_id const id)
		{
			remove_range({ id, id });
		}

		// Remove an entity from the component pool. This logically removes the component from the entity.
		void remove_range(entity_range const range)
		{
			Expects(has_entity_range(range));
			Expects(!is_queued_remove_range(range));

			if (deferred_removes->size() > 0 && deferred_removes->back().can_merge(range)) {
				deferred_removes->back() = entity_range::merge(deferred_removes->back(), range);
			}
			else {
				deferred_removes->push_back(range);
			}
		}

		// Returns an entities component
		// Pre: entity must have an component in this pool
		T* find_component_data([[maybe_unused]] entity_id const id) const
		{
			// TODO tagged+shared components could just return the address of a static var. 0 mem allocs
			if constexpr (is_shared_v<T> || is_tagged_v<T>) {
				// All entities point to the same component
				return get_shared_component<T>();
			}
			else {
				auto const index = find_entity_index(id);
				Expects(index != -1);
				return at(index);
			}
		}

		// Returns true if this pool had components added or removed
		bool was_changed() const noexcept override
		{
			return get_flags() != modified_state::none;
		}

		// Merge all the components queued for addition to the main storage,
		// and remove components queued for removal
		void process_changes() override
		{
			process_remove_components();
			process_add_components();
		}

		// Returns the number of active entities in the pool
		size_t num_entities() const noexcept
		{
			return std::accumulate(ranges.begin(), ranges.end(), size_t{ 0 }, [](size_t val, entity_range const& range) { return val + range.count(); });
		}

		// Returns the number of active components in the pool
		size_t num_components() const noexcept
		{
			return data.size();
		}

		// Returns the flag describing the state of the pool
		modified_state get_flags() const noexcept
		{
			return static_cast<modified_state>(state_);
		}

		// Clears the pools state flags
		void clear_flags() noexcept override
		{
			state_ = modified_state::none;
		}

		// Returns true if a certain flag is set
		bool has_flag(modified_state flag) const noexcept
		{
			return (state_ & flag) == flag;
		}

		// Sets a flag
		void set_flag(modified_state flag) noexcept
		{
			state_ |= flag;
		}

		// Returns the pools entities
		std::vector<entity_range> const& get_entities() const noexcept override
		{
			return ranges;
		}

		// Returns true if an entity has data in this pool
		bool has_entity(entity_id const id) const
		{
			return has_entity_range({ id, id });
		}

		// Returns true if an entity range has data in this pool
		bool has_entity_range(entity_range const range) const noexcept
		{
			if (ranges.empty())
				return false;

			for (entity_range const r : ranges) {
				if (r.contains(range))
					return true;
			}

			return false;
		}

		// Checks the current threads queue for the entity
		bool is_queued_add(entity_id const id) const
		{
			return is_queued_add_range({ id, id });
		}

		// Checks the current threads queue for the entity
		bool is_queued_add_range(entity_range const range) const
		{
			if (deferred_adds->empty())
				return false;

			if constexpr (detail::has_unique_component_v<T>) {
				for (auto const& ents : *deferred_adds) {
					if (ents.first.contains(range))
						return true;
				}
			}
			else {
				for (auto const& ents : *deferred_adds) {
					if (ents.contains(range))
						return true;
				}
			}

			return false;
		}

		// Checks the current threads queue for the entity
		bool is_queued_remove(entity_id const id) const
		{
			return is_queued_remove_range({ id, id });
		}

		// Checks the current threads queue for the entity
		bool is_queued_remove_range(entity_range const range) const
		{
			if (deferred_removes->empty())
				return false;

			for (auto const& ents : deferred_removes.get()) {
				if (ents.contains(range))
					return true;
			}

			return false;
		}

		// Clear all entities from the pool
		void clear() override
		{
			ranges.clear();
			data.clear();

			deferred_adds.clear();
			deferred_removes.clear();
			clear_flags();
		}

	private:

		// Returns the component at the specific index.
		// Pre: index is within bounds
		T* at(gsl::index const index) const
		{
			Expects(index >= 0 && index < static_cast<gsl::index>(data.size()));
			GSL_SUPPRESS(bounds.4)
			GSL_SUPPRESS(bounds.1)
			return data.data() + index;
		}

		// Searches for an entitys offset in to the component pool. Returns -1 if not found
		gsl::index find_entity_index(entity_id const ent) const
		{
			if (ranges.size() > 0) {
				// Run through the ranges
				gsl::index index = 0;
				for (entity_range const range : ranges) {
					if (!range.contains(ent)) {
						index += range.count();
						continue;
					}

					index += range.offset(ent);
					return index;
				}
			}

			// not found
			return -1;
		}

		// Adds new queued components to the main storage
		void process_add_components()
		{
			std::vector<entity_data> adds = deferred_adds.combine_inline([](auto &left, auto right) {
				std::move(right.begin(), right.begin() + right.size(), std::back_inserter(left));
			});
			if (adds.empty())
				return;

			// Clear the current adds
			deferred_adds.clear();

			// Sort the input
			auto constexpr comparator = [](entity_data const& l, entity_data const& r) {
				if constexpr (detail::has_unique_component_v<T>)
					return l.first.first() < r.first.first();
				else
					return l.first() < r.first();
			};
			if (!std::is_sorted(adds.begin(), adds.end(), comparator))
				std::sort(adds.begin(), adds.end(), comparator);

			// Small helper function for combining ranges
			auto const add_range = [](std::vector<entity_range> &dest, entity_range range) {
				// Merge the range or add it
				if (dest.size() > 0 && dest.back().can_merge(range)) {
					dest.back() = entity_range::merge(dest.back(), range);
				}
				else {
					dest.push_back(range);
				}
			};

			// Add the new components
			if constexpr (detail::has_unique_component_v<T>) {
				std::vector<entity_range> new_ranges;
				auto ranges_it = ranges.cbegin();
				auto component_it = data.begin();
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
					auto const add_val = [this, &component_it, range](T const& val) {
						component_it = data.insert(component_it, range.count(), val);
						component_it += range.count();
					};
					auto const add_init = [this, &component_it, range](std::function<T(entity_id)> init) {
						for (entity_id ent = range.first(); ent <= range.last(); ++ent.id) {
							component_it = data.insert(component_it, init(ent));
							component_it += 1;
						}
					};
					std::visit(detail::overloaded{add_val, add_init}, component_val);
				}

				// Copy the remaining ranges
				std::copy(ranges_it, ranges.cend(), std::back_inserter(new_ranges));

				// Store the new ranges
				ranges = std::move(new_ranges);
			}
			else {
				if (ranges.empty()) {
					ranges = std::move(adds);
				}
				else {
					// Do the inserts
					std::vector<entity_range> new_ents;
					auto ent = ranges.cbegin();
					for (auto const range : adds) {
						// Copy the current ranges while looking for an insertion point
						while (ent != ranges.cend() && !(*ent).contains(range)) {
							new_ents.push_back(*ent++);
						}

						// Merge the range or add it
						add_range(new_ents, range);
					}

					// Move the remaining entities
					while (ent != ranges.cend())
						new_ents.push_back(*ent++);

					ranges = std::move(new_ents);
				}
			}

			// Update the state
			set_flag(modified_state::add);
		}

		// Removes all marked components from their entities
		void process_remove_components()
		{
			// Transient components are removed each cycle
			if constexpr (std::is_base_of_v<ecs::transient, T>) {
				if (ranges.size() > 0) {
					ranges.clear();
					data.clear();
					set_flag(modified_state::remove);
				}
			}
			else {
				// Combine the vectors
				std::vector<entity_range> removes = deferred_removes.combine([](auto left, auto right) {
					left.insert(left.end(), right.begin(), right.end());
					return left;
					});
				if (removes.empty())
					return;

				// Clear the current removes
				deferred_removes.clear();

				// Sort it if needed
				if (!std::is_sorted(removes.begin(), removes.end()))
					std::sort(removes.begin(), removes.end());

				// Erase the ranges data
				if constexpr (detail::has_unique_component_v<T>) {
					auto dest_it = data.begin() + find_entity_index(removes[0].first());
					auto from_it = dest_it + removes[0].count();

					if (dest_it == data.begin() && from_it == data.end()) {
						data.clear();
					}
					else {
						// Move data between the ranges
						for (auto it = removes.begin() + 1; it != removes.end(); ++it) {
							auto const start_it = data.begin() + find_entity_index(it->first());
							while (from_it != start_it)
								*dest_it++ = std::move(*from_it++);
						}

						// Move rest of data
						while (from_it != data.end())
							*dest_it++ = std::move(*from_it++);

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
						auto const [range, split] = entity_range::remove(*curr_range, *it);

						// Update the modified range
						*curr_range = range;

						// If the range was split, add the other part of the range
						if (split)
							curr_range = ranges.insert(curr_range + 1, split.value());
					}
				}

				// Update the state
				set_flag(modified_state::remove);
			}
		}

	private:
		// The components data
		mutable std::vector<T> data; // mutable so the constants getters can return components whos contents can be modified

		// The entities that have data in this storage.
		std::vector<entity_range> ranges;

		// The type used to store data.
		using component_val = std::variant<T, std::function<T(entity_id)>>;
		using entity_data = std::conditional_t<detail::has_unique_component_v<T>, std::pair<entity_range, component_val>, entity_range>;

		// Keep track of which components to add/remove each cycle
		threaded<std::vector<entity_data>> deferred_adds;
		threaded<std::vector<entity_range>> deferred_removes;

		// 
		unsigned state_ = modified_state::none;
	};
}

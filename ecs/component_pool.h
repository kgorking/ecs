#pragma once
#include <gsl/gsl>
#include <gsl/span>
#include <variant>
#include <utility>

#include "../threaded/threaded/threaded.h"
#include "component_pool_base.h"
#include "component_specifier.h"
#include "entity_range.h"
#include "function.h"

namespace ecs::detail
{
	// True if each entity has their own unique component (ie. not shared across components)
	template <class T> constexpr bool has_unique_component_v = !(is_shared_v<T> || is_tagged_v<T>);

	// For std::binary_search
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
		component_pool() = default;
		component_pool(component_pool const&) = delete;
		component_pool(component_pool &&) = default;
		component_pool& operator =(component_pool const&) = delete;
		component_pool& operator =(component_pool &&) = default;

		// Adds a component to an entity.
		// Pre: entity has not already been added, or is in queue to be added
		void add(entity_id const id, T component)
		{
			add_range(id, id, std::move(component));
		}

		template <typename Fn>
		void add_range_init(entity_id const first, entity_id const last, Fn init)
		{
			static_assert(!(is_tagged_v<T> && sizeof(T) > 1), "Tagged components can not have any data in them");
			Expects(!has_entity_range(first, last));		// Entity already has this component
			Expects(!is_queued_add_range(first, last));		// Component already added this cycle

			if constexpr (is_shared_v<T> || is_tagged_v<T>) {
				// Shared/tagged components will all point to the same instance, so only allocate room for 1 component
				if (data.size() == 0) {
					data.emplace_back(init(0));
				}

				deferred_adds->emplace_back(entity_range{ first, last });
			}
			else {
				// Add the id and data to a temp storage
				deferred_adds->emplace_back(entity_range{ first, last }, detail::function_fix<T(ecs::entity_id)>{ init });
			}
		}

		// Adds a component to a range of entity.
		// Pre: entity has not already been added, or is in queue to be added
		void add_range(entity_id const first, entity_id const last, T component)
		{
			static_assert(!(is_tagged_v<T> && sizeof(T) > 1), "Tagged components can not have any data in them");
			Expects(!has_entity_range(first, last));		// Entity already has this component
			Expects(!is_queued_add_range(first, last));		// Component already added this cycle

			if constexpr (is_shared_v<T> || is_tagged_v<T>) {
				// Shared/tagged components will all point to the same instance, so only allocate room for 1 component
				if (data.size() == 0) {
					data.emplace_back(std::move(component));
				}

				deferred_adds->emplace_back(entity_range{ first, last });
			}
			else {
				// Add the id and data to a temp storage
				//deferred_adds->reserve(deferred_adds->size() + 1);
				deferred_adds->emplace_back(entity_range{ first, last }, std::move(component));
			}
		}

		// Returns the shared component
		template <typename = std::enable_if_t<is_shared_v<T>>>
		T* get_shared_component()
		{
			if (data.size() == 0) {
				data.emplace_back(T{});
			}

			return at(0);
		}

		// Remove an entity from the component pool. This logically removes the component from the entity.
		void remove(entity_id const id)
		{
			remove_range(id, id);
		}

		// Remove an entity from the component pool. This logically removes the component from the entity.
		void remove_range(entity_id const first, entity_id const last)
		{
			Expects(has_entity_range(first, last));
			Expects(!is_queued_remove_range(first, last));
			deferred_removes->push_back(entity_range{ first, last });
		}

		// Returns an entities component
		// Pre: entity must have an component in this pool
		T const* find_component_data([[maybe_unused]] entity_id const id) const
		{
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
		// entity_iterator find_entity_start(entity_id const ent)
		// entity_iterator find_entity_next(entity_iterator it, entity_id const ent)
		//using entity_iterator = std::vector<entity_id>::const_iterator;

		// Returns an entities component
		// Pre: entity must have a component in this pool
		T* find_component_data([[maybe_unused]] entity_id const id)
		{
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
			// Transient components are removed each cycle
			if constexpr (std::is_base_of_v<ecs::transient, T>) {
				if (ranges.size() > 0) {
					ranges.clear();
					data.clear();
					set_flag(modified_state::remove);
				}
			}

			process_add_components();
			process_remove_components();
		}

		// Returns the number of active entities in the pool
		size_t num_entities() const noexcept
		{
			return ranges.size();
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

		// Returns a view of the pools entities
		gsl::span<entity_range const> get_entities() const noexcept override
		{
			return gsl::make_span(ranges);
		}

		// Returns true if an entity has data in this pool
		bool has_entity(entity_id const id) const
		{
			return has_entity_range(id, id);
		}

		// Returns true if an entity range has data in this pool
		bool has_entity_range(entity_id const first, entity_id const last) const noexcept
		{
			if (ranges.empty())
				return false;

			for (entity_range const range : ranges) {
				if (range.contains(entity_range{ first, last }))
					return true;
			}

			return false;
		}

		// Checks the current threads queue for the entity
		bool is_queued_add(entity_id const id) const
		{
			return is_queued_add_range(entity_range{ id, id });
		}

		// Checks the current threads queue for the entity
		bool is_queued_add_range(entity_id const first, entity_id const last) const
		{
			if (deferred_adds->empty())
				return false;

			if constexpr (detail::has_unique_component_v<T>) {
				for (auto const& ents : *deferred_adds) {
					if (ents.first.contains(entity_range{ first, last }))
						return true;
				}
			}
			else {
				for (auto const& ents : *deferred_adds) {
					if (ents.contains(entity_range{ first, last }))
						return true;
				}
			}

			return false;
		}

		// Checks the current threads queue for the entity
		bool is_queued_remove(entity_id const id) const
		{
			return is_queued_remove_range(id, id);
		}

		// Checks the current threads queue for the entity
		bool is_queued_remove_range(entity_id const first, entity_id const last) const
		{
			if (deferred_removes->empty())
				return false;

			for (auto const& ents : deferred_removes.get()) {
				if (ents.contains(entity_range{ first, last }))
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

		// Returns the component at the specific index. Does not check bounds
		T const* at(gsl::index const index) const
		{
			GSL_SUPPRESS(bounds.4)
			return &data[index];
		}

		// Returns the component at the specific index. Does not check bounds
		T* at(gsl::index const index)
		{
			GSL_SUPPRESS(bounds.4)
			return &data[index];
		}

		// Searches for an entitys offset in to the component pool. Returns -1 if not found
		gsl::index find_entity_index(entity_id const ent) const noexcept
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
					auto const add_init = [this, &component_it, range](detail::function_fix<T(entity_id)> init) {
						for (entity_id ent = range.first(); ent.id <= range.last().id; ++ent.id) {
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

			// Holds the offsets (with adjustments) in 'data' that needs to be erased
			using data_range = std::pair<gsl::index, gsl::index>;
			[[maybe_unused]] std::vector<data_range> data_ranges;

			// Find the offsets in to the data storage
			if constexpr (detail::has_unique_component_v<T>) {
				data_ranges.reserve(removes.size());

				// If more than one range is removed, use this to adjust their indices into the 'data' vector
				size_t data_adjust = 0;

				for (auto it = removes.begin(); it != removes.end(); ++it) {
					data_ranges.emplace_back(
						find_entity_index(it->first()) - data_adjust,
						find_entity_index(it->last()) - data_adjust
					);

					// Adjust the data offset
					data_adjust += it->count();
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

			// Remove the corresponding ranges of data
			if constexpr (detail::has_unique_component_v<T>) {
				for (auto const [first, last] : data_ranges) {
					if (first == last)
						data.erase(data.begin() + first);
					else
						data.erase(data.begin() + first, data.begin() + last);
				}
			}

			// Update the state
			set_flag(modified_state::remove);
		}

	private:
		// The components data
		std::vector<T> data;

		// The entities that have data in this storage.
		std::vector<entity_range> ranges;

		// The type used to store data.
		using component_val = std::variant<T, detail::function_fix<T(entity_id)>>;
		using entity_data = std::conditional_t<detail::has_unique_component_v<T>, std::pair<entity_range, component_val>, entity_range>;

		// Keep track of which components to add/remove each cycle
		threaded<std::vector<entity_data>> deferred_adds;
		threaded<std::vector<entity_range>> deferred_removes;

		// 
		unsigned state_ = modified_state::none;
	};
}

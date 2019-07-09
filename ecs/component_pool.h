#pragma once
#include <gsl/gsl>
#include <gsl/span>

#include "../threaded/threaded/threaded.h"
#include "component_pool_base.h"
#include "component_specifier.h"

namespace ecs::detail
{
	// True if each entity has their own component
	template <class T> constexpr bool has_component_v = !(is_shared_v<T> || is_tagged_v<T>);

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
			static_assert(!(is_tagged_v<T> && sizeof(T) > 1), "Tagged components can not have any data in them");
			Expects(!has_entity(id));		// Entity already has this component
			Expects(!is_queued_add(id));	// Component already added this cycle

			if constexpr (is_shared_v<T> || is_tagged_v<T>) {
				// Shared/tagged components will all point to the same instance, so only allocate room for 1 component
				if (data.size() == 0) {
					data.emplace_back(std::move(component));
				}

				deferred_adds->emplace_back(id);
			}
			else {
				// Add the id and data to a temp storage
				deferred_adds->emplace_back(id, std::move(component));
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
			Expects(has_entity(id));
			Expects(!is_queued_remove(id));
			deferred_removes->push_back(id);
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
				if (entities.size() > 0) {
					entities.clear();
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
			return entities.size();
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
		gsl::span<entity_id const> get_entities() const noexcept override
		{
			return gsl::make_span(entities);
		}

		// Returns true if an entity has data in this pool
		bool has_entity(entity_id const id) const
		{
			if (entities.empty())
				return false;

			return -1 != find_entity_index(id);
		}

		// Checks the current threads queue for the entity
		bool is_queued_add(entity_id const id) const
		{
			if (deferred_adds->empty())
				return false;

			if constexpr (detail::has_component_v<T>) {
				return std::binary_search(deferred_adds->begin(), deferred_adds->end(), id, detail::overloaded{
					[](entity_temp const& a, entity_id b) { return a.first.id < b.id; },
					[](entity_id const& a, entity_temp b) { return a.id < b.first.id; } });
			}
			else {
				return std::binary_search(deferred_adds->begin(), deferred_adds->end(), id);
			}
		}

		// Checks the current threads queue for the entity
		bool is_queued_remove(entity_id const id) const
		{
			if (deferred_removes->empty())
				return false;

			return std::binary_search(deferred_removes->begin(), deferred_removes->end(), id);
		}

		// Clear all entities from the pool
		void clear() override
		{
			entities.clear();
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

		// Searches for an entity in the component pool. Returns -1 if not found
		// TODO make this faster
		gsl::index find_entity_index(entity_id const ent) const
		{
			// Try a constant-time lookup
			GSL_SUPPRESS(bounds.4) { // suppress warning about unbounded lookup
				if (ent.id < entities.size() && ent == entities[ent.id])
					return ent.id;
			}

			// Do a binary search for the entity location
			auto const it = std::lower_bound(entities.cbegin(), entities.cend(), ent);
			if (it != entities.cend() && ent == *it)
				return std::distance(entities.cbegin(), it);
			else
				return -1;
		}

		// Adds new queued components to the main storage
		void process_add_components()
		{
			// Combine the vectors
			std::vector<entity_temp> adds = deferred_adds.combine([](auto left, auto right) {
				left.insert(left.end(), right.begin(), right.end());
				return left;
			});
			if (adds.empty())
				return;

			// Clear the current adds
			deferred_adds.clear();

			// Sort the input
			auto constexpr comparator = [](entity_temp const& l, entity_temp const& r) {
				if constexpr (detail::has_component_v<T>)
					return l.first < r.first;
				else
					return l < r;
			};
			if (!std::is_sorted(std::execution::seq, adds.begin(), adds.end(), comparator))
				std::sort(std::execution::seq, adds.begin(), adds.end(), comparator);

			// Add the new components
			if constexpr (detail::has_component_v<T>) {
				if (entities.empty()) {
					size_t const new_size = num_entities() + adds.size();
					if (new_size > entities.capacity()) {
						entities.reserve(new_size);
						if constexpr (detail::has_component_v<T>)
							data.reserve(new_size);
					}

					for (entity_temp & pair : adds) {
						entities.emplace_back(pair.first);
						data.emplace_back(std::move(pair.second));
					}
				}
				else {
					size_t const new_size = entities.size() + adds.size();

					std::vector<entity_id> new_ents;
					std::vector<T> new_data;
					new_ents.reserve(new_size);
					new_data.reserve(new_size);

					// Do the inserts
					auto ent_id = entities.cbegin();
					auto component = data.begin();
					for (auto ins = adds.begin(); ins != adds.end(); ++ins) {
						auto &[id, t] = *ins;

						// Shift the vector while looking for an insertion point
						while (ent_id != entities.cend() && (*ent_id).id < id.id) {
							new_ents.emplace_back(*ent_id++);
							new_data.emplace_back(std::move(*component++));
						}

						// Add the new entity id and component
						new_ents.emplace_back(id);
						new_data.emplace_back(std::move(t));
					}

					entities = std::move(new_ents);
					data = std::move(new_data);
				}
			}
			else {
				if (entities.empty()) {
					entities = std::move(adds);
				}
				else {
					size_t const new_size = entities.size() + adds.size();

					std::vector<entity_id> new_ents;
					new_ents.reserve(new_size);

					// Do the inserts
					auto ent = entities.cbegin();
					for (auto ins = adds.cbegin(); ins != adds.cend(); ++ins) {
						auto const id = *ins;

						// Shift the vector while looking for an insertion point
						while (ent != entities.cend() && *ent < id) {
							new_ents.push_back(*ent++);
						}

						// Add the new entity id and component
						new_ents.push_back(id);
					}

					entities = std::move(new_ents);
				}
			}

			// Update the state
			set_flag(modified_state::add);
		}

		// Removes all marked components from their entities
		void process_remove_components()
		{
			// Combine the vectors
			std::vector<entity_id> removes = deferred_removes.combine([](auto left, auto right) {
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

			// Remove the entities
			auto const it_entity_end = std::remove_if(entities.begin(), entities.end(), [this, removes_end = removes.end(), ent_to_remove = removes.begin()](auto const& ent) mutable {
				if (ent_to_remove != removes_end && ent == *ent_to_remove) {
					if constexpr (detail::has_component_v<T>) {
						// Get the offset of this entity into the 'entities' vector
						// and use that offset to remove components from 'data'
						ptrdiff_t const offset = (&ent - entities.data());

						// Save that offset in the current position in the 'removes' vector.
						ent_to_remove->id = gsl::narrow<std::uint32_t>(offset);
					}

					// Step forward to find the next entity
					++ent_to_remove;
					return true;
				}
				else
					return false;
			});
			entities.erase(it_entity_end, entities.end());

			// 'removes' now holds all the offsets of components to remove
			if constexpr (detail::has_component_v<T>) {
				auto const it_data_end = std::remove_if(data.begin(), data.end(), [this, removes_end = removes.end(), component_to_remove = removes.begin()](auto &comp) mutable {
					if (component_to_remove != removes_end) {
						ptrdiff_t const offset = &comp - data.data();
						if (offset == (*component_to_remove).id) {
							++component_to_remove;
							return true;
						}
					}

					return false;
				});
				data.erase(it_data_end, data.end());
			}

			// Update the state
			set_flag(modified_state::remove);
		}

	private:
		// The components data
		std::vector<T> data;

		// The entities that have data in this storage.
		// TODO store ranges instead
		std::vector<entity_id> entities;
		using entity_iterator = std::vector<entity_id>::const_iterator;

		// The type used to store data.
		using entity_temp = std::conditional_t<detail::has_component_v<T>, std::pair<entity_id, T>, entity_id>;

		// Keep track of which components to add/remove each cycle
		threaded<std::vector<entity_temp>> deferred_adds;
		threaded<std::vector<entity_id>> deferred_removes;

		// 
		unsigned state_ = modified_state::none;
	};
}

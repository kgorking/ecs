#pragma once
#include <utility>
#include <algorithm>
#include <type_traits>
#include <gsl/gsl>
#include "system.h"
#include "component_pool.h"
#include "runtime.h"
#include "entity.h"

namespace ecs::detail
{
	// The implementation of a system specialized on its components
	template <class ExecutionPolicy, typename UpdatePrototype, class FirstComponent, class ...Components>
	class system_impl final : public system
	{
		// Determines if the first component is an entity
		static constexpr bool is_first_component_entity = std::is_same_v<FirstComponent, entity_id> || std::is_same_v<FirstComponent, entity>;

		// Calculate the number of components
		static constexpr size_t num_components = sizeof...(Components) + (is_first_component_entity ? 0 : 1);

		// The first type in the system, entity or component
		using first_type = std::conditional_t<is_first_component_entity, FirstComponent, FirstComponent*>;

		// Holds the arguments for the systems
		using compact_arg = std::conditional_t<is_first_component_entity, 
			std::vector<std::tuple<entity_range,                  Components* ...>>,
			std::vector<std::tuple<entity_range, FirstComponent*, Components* ...>>>;
		compact_arg compact_args;

		// A tuple of the fully typed component pools used by this system
		using tup_pools = std::conditional_t<is_first_component_entity,
			std::tuple<                                 component_pool<Components>&...>,
			std::tuple<component_pool<FirstComponent>&, component_pool<Components>&...>>;
		tup_pools const pools;

		// The user supplied system
		UpdatePrototype const update_func;

	public:
		// Constructor for when the first argument to the system is _not_ an entity
		system_impl(UpdatePrototype update_func, component_pool<FirstComponent> & first_pool, component_pool<Components> &... pools)
			: pools{ first_pool, pools... }
			, update_func{ update_func }
		{
			build_args();
		}

		// Constructor for when the first argument to the system _is_ an entity
		system_impl(UpdatePrototype update_func, component_pool<Components> &... pools)
			: pools{ pools... }
			, update_func{ update_func }
		{
			build_args();
		}

		void update() override
		{
			for (auto &arg : compact_args) {
				auto range = std::get<entity_range>(arg);
				std::for_each(ExecutionPolicy{}, range.begin(), range.end(), [this, &arg, first_id = range.first().id](auto ent) {
					ptrdiff_t const offset = ent.id - first_id;

					if constexpr (is_first_component_entity) {
						update_func(ent, *extract_arg(std::get<Components*>(arg), offset)...);
					}
					else {
						update_func(*extract_arg(std::get<FirstComponent*>(arg), offset), *extract_arg(std::get<Components*>(arg), offset)...);
					}
				});
			}
		}

		// Handle changes when the component pools change
		void process_changes() override
		{
			// Leave if nothing has changed
			bool modified = (get_pool<Components>().was_changed() || ... || false);
			if (!modified) {
				if constexpr (!is_first_component_entity) {
					if (!get_pool<FirstComponent>().was_changed())
						return;
				}
				else
					return;
			}

			build_args();
		}

	protected:
		void build_args()
		{
			std::vector<entity_range> const& entities_set = std::get<0>(pools).get_entities();

			if constexpr (num_components == 1)
			{
				// Build the arguments
				build_args(entities_set);
			}
			else
			{
				// When there are more than one component required for a system,
				// find the intersection of the sets of entities that have those components
				
				// Hold the ranges that intersect
				std::vector<entity_range> intersect(entities_set.begin(), entities_set.end());

				// Intersects two ranges of entities
				auto const intersector = [](std::vector<ecs::entity_range> const& vec_a, gsl::span<ecs::entity_range const> vec_b) -> std::vector<ecs::entity_range> {
					std::vector<ecs::entity_range> result;

					if (vec_a.empty() || vec_b.empty())
						return result;

					auto it_a = vec_a.begin();
					auto it_b = vec_b.begin();

					while (it_a != vec_a.end() && it_b != vec_b.end()) {
						if (it_a->overlaps(*it_b))
							result.push_back(ecs::entity_range::intersect(*it_a, *it_b));

						if (it_a->last() < it_b->last()) // range a is inside range b, move to the next range in a
							++it_a;
						else if (it_b->last() < it_a->last()) // range b is inside range a, move to the next range in b
							++it_b;
						else { // ranges are equal, move to next ones
							++it_a;
							++it_b;
						}
					}

					return result;
				};

				// Find the intersections of all the pools
				((intersect = intersector(intersect, get_pool<Components>().get_entities())), ...);

				// Build the arguments
				build_args(intersect);
			}
		}

		// Convert a set of entities into arguments that can be passed to the system
		void build_args(std::vector<entity_range> const& entities)
		{
			// Build the compact arguments
			compact_args.clear();
			for (auto range : entities) {
				if constexpr (is_first_component_entity)
					compact_args.emplace_back(range,                                               get_component<Components>(range.first())...);
				else
					compact_args.emplace_back(range, get_component<FirstComponent>(range.first()), get_component<Components>(range.first())...);
			}
		}

		template <typename Component>
		Component* extract_arg(Component* ptr, [[maybe_unused]] ptrdiff_t offset)
		{
			if constexpr (has_unique_component_v<Component>)
				return ptr + offset;
			else
				return ptr;
		}

		template <typename Component>
		component_pool<Component>& get_pool() const noexcept
		{
			return std::get<component_pool<Component>&>(pools);
		}

		template <typename Component>
		Component* get_component(entity_id const entity)
		{
			return get_pool<Component>().find_component_data(entity);
		}
	};
}

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
	template <class...> struct null_v : std::integral_constant<int, 0> {};

	// The implementation of a system specialized on its components
	template <
		class ExecutionPolicy,
		typename UpdatePrototype,
		class FirstComponent,
		class ...Components>
	class system_impl final : public system
	{
		// Determines if the first component is an entity identifier
		static constexpr bool has_entity = std::is_same_v<FirstComponent, entity_id> || std::is_same_v<FirstComponent, entity>;

		// Calculates the number of components
		static constexpr size_t num_components = sizeof...(Components) + (has_entity ? 0 : 1);

		using FirstType = std::conditional_t<has_entity, entity_id, FirstComponent*>;
		using system_args = std::tuple<std::vector<FirstType>, std::vector<Components*>... >;

		// A tuple of the fully typed component pools used by this system
		using tup_pools = std::conditional_t<has_entity,
			std::tuple<                                 component_pool<Components>&...>,	// FirstComponent is an entity(_id), so don't include it in the pools
			std::tuple<component_pool<FirstComponent>&, component_pool<Components>&...>		// FirstComponent is a component, so include it in the pools
		>;

		system_args arguments;
		tup_pools const pools;
		UpdatePrototype const update_func;

	public:
		// Constructor for when the first argument to the system is _not_ an entity
		system_impl(
			UpdatePrototype update_func,
			component_pool<FirstComponent> & first_pool,
			component_pool<Components> &... pools
		) noexcept
			: pools{ first_pool, pools... }
			, update_func{ update_func }
		{ }

		// Constructor for when the first argument to the system _is_ an entity
		system_impl(
			UpdatePrototype update_func,
			component_pool<Components> &... pools
		) noexcept
			: pools{ pools... }
			, update_func{ update_func }
		{ }

		void update() noexcept override
		{
			std::tuple const iters = std::make_tuple(
				std::get<std::vector<Components*>>(arguments).begin()...);

			auto const& first = std::get<0>(arguments);
			auto const* first_data = first.data();
			std::for_each(ExecutionPolicy{}, first.begin(), first.end(), [first_data, iters, this](auto & f) {
				// Calculate the offset into the arrays
				ptrdiff_t const offset = &f - first_data;

				// Handle systems that take an 'ecs::entity' as their first argument
				if constexpr (std::is_same_v<ecs::entity, FirstComponent>) {
					update_func(entity{ f }, (**(std::get<std::vector<Components*>::iterator>(iters) + offset))...);
				}
				// Handle systems that take an 'ecs::entity_id' as their first argument
				else if constexpr (std::is_same_v<ecs::entity_id, FirstComponent>) {
					update_func(f, (**(std::get<std::vector<Components*>::iterator>(iters) + offset))...);
				}
				// Handle systems that just take components
				else {
					update_func(*f, (**(std::get<std::vector<Components*>::iterator>(iters) + offset))...);
				}
			});
		}

		// Handle changes when the component pools change
		void process_changes() override
		{
			// Leave if nothing has changed
			bool const modified = (get_pool<Components>().was_changed() || ... || false);

			if constexpr (!has_entity)
			{
				bool const first_modified = get_pool<FirstComponent>().was_changed();
				if (!(modified || first_modified)) {
					return;
				}
			}
			else
			{
				if (!modified) {
					return;
				}
			}

			gsl::span<entity_id const> const entities_set = std::get<0>(pools).get_entities();
			if constexpr (num_components == 1)
			{
				// Build the arguments
				build_args(entities_set);
			}
			else
			{
				auto const intersector = [](auto & intersect, auto const set) {
					if (set.empty()) {
						intersect.clear();
						return;
					}

					// This is just an in-place set intersection
					auto new_end = std::remove_if(intersect.begin(), intersect.end(), [it = set.begin(), last = std::prev(set.end())](entity_id ent) mutable {
						while (it != last && *it < ent)
							++it;
						return !((*it) == ent);
					});
					intersect.erase(new_end, intersect.end());
				};

				// Find the intersections of all the pools
				std::vector<entity_id> intersect(entities_set.cbegin(), entities_set.cend());
				(intersector(intersect, get_pool<Components>().get_entities()), ...);

				// Build the arguments
				build_args(gsl::make_span(intersect));
			}
		}

	protected:
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

		// Convert a set of entities into arguments that can be passed to the system
		void build_args(gsl::span<entity_id const> cont)
		{
			if constexpr (has_entity)
				std::get<0>(arguments).assign(cont.begin(), cont.end());
			else
				std::get<std::vector<FirstComponent*>>(arguments).resize(cont.size());
			(std::get<std::vector<Components*>>(arguments).resize(cont.size()), ...);

			// Get the pointers to where the arguments will be stored
			std::tuple arg_data = std::make_tuple(
				std::get<std::vector<FirstType>>(arguments).data(),
				std::get<std::vector<Components*>>(arguments).data()...);

			// Extract the data
			for (entity_id const id : cont) {
				// Gets the iterator where the component will be stored from the tuple,
				// dereferences it,
				// assigns the entitys component pointer to it,
				// increment the iterator
				if constexpr (!has_entity) {
					GSL_SUPPRESS(bounds.1) *std::get<FirstComponent**>(arg_data)++ = get_component<FirstComponent>(id);
				}
				GSL_SUPPRESS(bounds.1) ((*std::get<Components**>(arg_data)++ = get_component<Components>(id)), ...);
			}
		}
	};
}

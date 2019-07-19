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
	template <class ExecutionPolicy, typename UpdatePrototype, class FirstComponent, class ...Components>
	class system_impl final : public system
	{
		// Determines if the first component is an entity identifier
		static constexpr bool is_first_component_entity = std::is_same_v<FirstComponent, entity_id> || std::is_same_v<FirstComponent, entity>;

		// Calculates the number of components
		static constexpr size_t num_components = sizeof...(Components) + (is_first_component_entity ? 0 : 1);

		using FirstType = std::conditional_t<is_first_component_entity, entity_id, FirstComponent*>;
		using system_args = std::tuple<std::vector<FirstType>, std::vector<Components*>... >;

		// A tuple of the fully typed component pools used by this system
		using tup_pools = std::conditional_t<is_first_component_entity,
			std::tuple<                                 component_pool<Components>&...>,  // FirstComponent is an entity(_id), so don't include it in the pools
			std::tuple<component_pool<FirstComponent>&, component_pool<Components>&...>>; // FirstComponent is a component, so include it in the pools

		system_args arguments;
		tup_pools const pools;
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

			if constexpr (!is_first_component_entity) {
				bool const first_modified = get_pool<FirstComponent>().was_changed();
				if (!(modified || first_modified))
					return;
			}
			else {
				if (!modified)
					return;
			}

			build_args();
		}

	protected:
		void build_args()
		{
			gsl::span<entity_range const> const entities_set = std::get<0>(pools).get_entities();

			if constexpr (num_components == 1)
			{
				// Build the arguments
				build_args(entities_set);
			}
			else
			{
				// Hold the ranges that intersect
				std::vector<entity_range> intersect;

				// When there are more than one component required for a system,
				// find the intersection of the sets of entities that have those components
				auto const intersector = [&intersect, &entities_set](gsl::span<entity_range const> const& first, gsl::span<entity_range const> const other) {
					if (&first == &other)
						return;
					if (first.empty() || other.empty()) {
						intersect.clear();
						return;
					}

					// Trim the ranges in first using the ranges in other
					auto other_it = other.begin();
					auto it = first.begin();

					while (it != first.end() && other_it != other.end()) {
						entity_range other_range = *other_it;
						if (!it->overlaps(other_range)) {
							++it;
							continue;
						}

						// Do an intersect on the ranges or remove the range if they are identical
						if (it->equals(other_range))
							intersect.push_back(*it);
						else
							intersect.push_back(entity_range::intersect(*it, other_range));

						// Advance the other iterator
						++other_it;
					}
				};

				// Find the intersections of all the pools
				(intersector(entities_set, get_pool<Components>().get_entities()), ...);

				// Build the arguments
				if (!intersect.empty())
					build_args(gsl::make_span(intersect));
			}
		}

		// Convert a set of entities into arguments that can be passed to the system
		void build_args(gsl::span<entity_range const> entities)
		{
			// Count the number of entities
			auto num_entities = std::accumulate(entities.begin(), entities.end(), size_t{ 0 }, [](size_t val, entity_range const& range) { return val + range.count(); });

			if constexpr (is_first_component_entity) {
				// Copy the entities into the arguments
				auto& args = std::get<0>(arguments);
				args.resize(num_entities);
				auto dest = args.begin();
				for (entity_range const range : entities) {
					std::iota(dest, dest + range.count(), range.first().id);
					dest += range.count();
				}
			}
			else
				std::get<std::vector<FirstComponent*>>(arguments).resize(num_entities);
			(std::get<std::vector<Components*>>(arguments).resize(num_entities), ...);

			// Get the pointers to where the arguments will be stored
			std::tuple arg_data = std::make_tuple(
				std::get<std::vector<FirstType>>(arguments).data(),  // entity_id or component*
				std::get<std::vector<Components*>>(arguments).data()...);
			size_t arg_index = 0;

			// Extract the data
			for (entity_range const range : entities) {
				// Gets the iterator where the component will be stored from the tuple,
				// dereferences it,
				// assigns the entitys component pointer to it,
				// increment the iterator
				auto others = std::make_tuple(get_component<Components>(range.first())...);
				if constexpr (!is_first_component_entity) {
					auto first = get_component<FirstComponent>(range.first());

					for (int i = 0; i < range.count(); i++) {
						GSL_SUPPRESS(bounds.1) std::get<FirstComponent**>(arg_data)[arg_index + i] = extract_arg(first, i);
						GSL_SUPPRESS(bounds.1) ((std::get<Components**>(arg_data)[arg_index + i] = extract_arg(std::get<Components*>(others), i)), ...);
					}
				}
				else {
					for (int i = 0; i < range.count(); i++) {
						GSL_SUPPRESS(bounds.1) ((std::get<Components**>(arg_data)[arg_index + i] = extract_arg(std::get<Components*>(others), i)), ...);
					}
				}
				arg_index += range.count();
			}
		}

		template <typename Component>
		Component* extract_arg(Component* ptr, [[maybe_unused]] int index)
		{
			if constexpr (has_unique_component_v<Component>)
				return ptr + index;
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

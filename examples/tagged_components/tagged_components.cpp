#include <ecs/ecs.h>
#include <iostream>
#include <string>

struct flammable_t { using ecs_flags = ecs::flags<ecs::tag>; };
struct freezable_t { using ecs_flags = ecs::flags<ecs::tag>; };
struct shockable_t { using ecs_flags = ecs::flags<ecs::tag>; };

struct Name : std::string {};

int main() {
	ecs::runtime rt;

	// Add systems for each tag. Will just print the name.
	// Since tags never hold any data, they are always passed by value
	rt.make_system([](Name const &name, freezable_t) { std::cout << "  freezable: " << name << "\n"; });
	rt.make_system([](Name const &name, shockable_t) { std::cout << "  shockable: " << name << "\n"; });
	rt.make_system([](Name const &name, flammable_t) { std::cout << "  flammable: " << name << "\n"; });

	// Set up the entities with a name and tags
	rt.add_component(0, Name{"Jon"}, freezable_t{});
	rt.add_component(1, Name{"Sean"}, flammable_t{});
	rt.add_component(2, Name{"Jimmy"}, shockable_t{});
	rt.add_component(3, Name{"Rachel"}, flammable_t{}, freezable_t{}, shockable_t{});
	rt.add_component(4, Name{"Suzy"}, flammable_t{});

	// Commit the changes
	rt.commit_changes();
	std::cout << "Created 'Jon' with freezable_t\n"
				 "        'Sean' with flammable_t\n"
				 "        'Jimmy' with shockable_t\n"
				 "        'Rachel' with flammable_t, freezable_t, shockable_t\n"
				 "        'Suzy' with flammable_t\n\n";

	// Run the systems
	std::cout << "Running systems:\n";
	rt.run_systems();

	std::cout << "\nStat dump:\n";
	std::cout << "  Number of entities with the flammable_t tag: " << rt.get_entity_count<flammable_t>()
			  << "\n  Number of entities with the shockable_t tag: " << rt.get_entity_count<shockable_t>()
			  << "\n  Number of entities with the freezable_t tag: " << rt.get_entity_count<freezable_t>()
			  << "\n  Number of flammable_t components allocated:  " << rt.get_component_count<flammable_t>()
			  << "\n  Number of shockable_t components allocated:  " << rt.get_component_count<shockable_t>()
			  << "\n  Number of freezable_t components allocated:  " << rt.get_component_count<freezable_t>() << "\n";
}

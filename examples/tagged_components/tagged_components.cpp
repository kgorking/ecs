#include <ecs/ecs.h>
#include <iostream>
#include <string>

struct flameable_t {
	using ecs_flags = ecs::flags<ecs::tag>;
};
struct freezeable_t {
	using ecs_flags = ecs::flags<ecs::tag>;
};
struct shockable_t {
	using ecs_flags = ecs::flags<ecs::tag>;
};

struct Name : std::string {};

int main() {
	ecs::runtime rt;

	// Add systems for each tag. Will just print the name.
	// Since tags never hold any data, they are always passed by value
	rt.make_system<ecs::opts::group<0>>([](Name const &name, freezeable_t) { std::cout << "  freezeable: " << name << "\n"; });
	rt.make_system<ecs::opts::group<1>>([](Name const &name, shockable_t) { std::cout << "  shockable:  " << name << "\n"; });
	rt.make_system<ecs::opts::group<2>>([](Name const &name, flameable_t) { std::cout << "  flammeable: " << name << "\n"; });

	// Set up the entities with a name and tags
	rt.add_component(0, Name{"Jon"}, freezeable_t{});
	rt.add_component(1, Name{"Sean"}, flameable_t{});
	rt.add_component(2, Name{"Jimmy"}, shockable_t{});
	rt.add_component(3, Name{"Rachel"}, flameable_t{}, freezeable_t{}, shockable_t{});
	rt.add_component(4, Name{"Suzy"}, flameable_t{});

	// Commit the changes
	rt.commit_changes();
	std::cout << "Created 'Jon' with freezeable_t\n"
				 "        'Sean' with flameable_t\n"
				 "        'Jimmy' with shockable_t\n"
				 "        'Rachel' with flameable_t, freezeable_t, shockable_t\n"
				 "        'Suzy' with flameable_t\n\n";

	// Run the systems
	std::cout << "Running systems:\n";
	rt.run_systems();

	std::cout << "\nStat dump:\n";
	std::cout << "  Number of entities with the flameable_t tag:  " << rt.get_entity_count<flameable_t>()
			  << "\n  Number of entities with the shockable_t tag:  " << rt.get_entity_count<shockable_t>()
			  << "\n  Number of entities with the freezeable_t tag: " << rt.get_entity_count<freezeable_t>()
			  << "\n  Number of flameable_t components allocated:   " << rt.get_component_count<flameable_t>()
			  << "\n  Number of shockable_t components allocated:   " << rt.get_component_count<shockable_t>()
			  << "\n  Number of freezeable_t components allocated:  " << rt.get_component_count<freezeable_t>() << "\n";
}

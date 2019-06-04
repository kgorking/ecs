#include <iostream>
#include <string>
#include <ecs/ecs.h>

struct Flameable : ecs::tag {};
struct Freezeable : ecs::tag {};
struct Shockable: ecs::tag {};

struct Name : std::string {};

int main()
{
	try {
		// Add systems for each tag. Will just print the name
		ecs::add_system([](Name const& name, Freezeable&) { std::cout << "freezeable: " << name << "\n";  });
		ecs::add_system([](Name const& name, Shockable&)  { std::cout << "shockable:  " << name << "\n";  });
		ecs::add_system([](Name const& name, Flameable&)  { std::cout << "flammeable: " << name << "\n"; });

		// Set up the entities with a name and tags
		ecs::entity
			jon{ 0, Name{ "Jon" }, Freezeable{} },
			sean{ 1, Name{ "Sean" }, Flameable{} },
			jimmy{ 2, Name{ "Jimmy" }, Shockable{} },
			rachel{ 3, Name{ "Rachel" }, Flameable{}, Freezeable{}, Shockable{} },
			suzy{ 4, Name{ "Suzy" }, Flameable{} };

		// Commit the changes and run the systems
		ecs::update_systems();

		std::cout
			<< "\nNumber of entities with the Flameable tag:  " << ecs::get_entity_count<Flameable>()
			<< "\nNumber of entities with the Shockable tag:  " << ecs::get_entity_count<Shockable>()
			<< "\nNumber of entities with the Freezeable tag: " << ecs::get_entity_count<Freezeable>()
			<< "\nNumber of Flameable components allocated:   " << ecs::get_component_count<Flameable>()
			<< "\nNumber of Shockable components allocated:   " << ecs::get_component_count<Shockable>()
			<< "\nNumber of Freezeable components allocated:  " << ecs::get_component_count<Freezeable>() << "\n";
	}
	catch (std::exception const& e) {
		std::cout << e.what() << "\n";
	}
}

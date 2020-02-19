#include <iostream>
#include <string>
#include <ecs/ecs.h>


int main()
{
	std::cout << "# 1. A simple example\n";
	// A system that operates on entities that hold an 'int'
	ecs::make_system([](int const& i) {
		std::cout << i << '\n';
	});

	// Set up 3 entities with their components
	// This uses the entity_range class, which is just a
	// wrapper for the interface to allow easy usage
	ecs::entity_range ents{ 0, 2, int{1} };

	// Commit the changes and run the systems
	ecs::update_systems();



	std::cout << "\n# 2. using a lambda to initialize components\n";
	ecs::entity_range{ 3, 5, [](ecs::entity_id ent) -> int { return ent.id * 2; } };
	ecs::update_systems();



	std::cout << "\n# 3. Adding a second component\n";
	// Add another system that operates on entities that hold an 'int' and 'std::string'
	ecs::make_system([](int const& i, std::string const& s) {
		std::cout << i << ": " << s << '\n';
	});

	// Add a second component to the last 3 entities
	ecs::add_component(3, std::string{ "jon" });
	ecs::add_component(4, std::string{ "sean" });
	ecs::add_component(5, std::string{ "jimmy" });

	// Commit the changes and run the systems
	ecs::update_systems();



	std::cout << "\n# 4. Removing a component\n";
	// Remove the integer component from the 'sean' entity using the 'ecc::entity' helper class
	ecs::entity sean{ 4 };
	sean.remove<int>();		// same as ecs::remove_component<int>(4);

	// Commit the changes and run the systems
	ecs::update_systems();


	std::cout << "\n# 5. Accessing the entity id\n";
	ecs::make_system([](ecs::entity_id ent, std::string const& s) {
		std::cout << "entity with id " << ent.id << " is named " << s << '\n';
		});
	ecs::update_systems();
}

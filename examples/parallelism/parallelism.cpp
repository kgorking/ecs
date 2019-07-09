#include <iostream>
#include <ecs/ecs.h>
#include <ecs/entity_range.h>

// Component addition/removal during a system run is thread-safe

int main()
{
	try {
		// Manually allow floats to be added to entities.
		ecs::runtime::init_components<float>();

		// Add a system that processes components in parallel
		ecs::add_system_parallel([](ecs::entity e, int const&) {
			e.add<float>();
			e.remove<int>();
		});

		// Run the system
		std::cout << "Running system...";
		ecs::entity_range ents{ 0, 256 * 1024 - 1 };
		ents.add(int{ 0 });
		ecs::update_systems();
		std::cout << '\n';

		// Commit the float additions from the system
		std::cout << "Committing changes...";
		ecs::commit_changes();
		std::cout << '\n';

		// Check how many floats were added
		std::cout << ecs::get_component_count<float>() << " floats were added, expected " << ents.count() << '\n';
	}
	catch (std::exception const& e) {
		std::cout << e.what() << "\n";
	}
}

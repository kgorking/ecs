#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <ecs/ecs.h>
#include <ecs/entity_range.h>

using namespace std::chrono_literals;

int main()
{
	std::cout << "#1\n";
	// A system that operates on entities that hold an 'int'
	ecs::add_system([](int const& i) {
		std::cout << i << '\n';
	});

	// Set up some entities with their ids and components
	ecs::entity
		jon{ 0, 4 },
		sean{ 1, 8 },
		jimmy{ 2, 12 };

	// Commit the changes and run the systems
	ecs::update_systems();



	std::cout << "\n#2\n";
	// Add another system that operates on entities that hold an 'int' and 'std::string' (order is irrelevant)
	ecs::add_system([](int const& i, std::string const& s) {
		std::cout << i << ": " << s << '\n';
	});

	// Add a second component to the entities
	jon.add(std::string{ "jon" });
	sean.add(std::string{ "sean" });
	jimmy.add(std::string{ "jimmy" });

	// Commit the changes and run the systems
	ecs::update_systems();



	std::cout << "\n#3\n";
	// Remove the integer component from the 'sean' entity
	sean.remove<int>();

	// Commit the changes and run the systems
	ecs::update_systems();



	std::cout << "\n#4\n";
	ecs::add_system([](ecs::entity_id ent, std::string const& s) {
		std::cout << "entity with id " << ent.id << " is named " << s << '\n';
		});
	ecs::update_systems();


	std::cout << "\n#5\n";
	ecs::runtime::reset(); // remove the existing systems and components

	// The lambda used by both the serial- and parallel systems
	auto constexpr sys_sleep = [](short const&) {
		std::this_thread::sleep_for(10ms);
	};

	// Add the systems
	auto& serial_sys   = ecs::add_system(sys_sleep);
	auto& parallel_sys = ecs::add_system_parallel(sys_sleep);

	// Create a range of 500 entites that would
	// take 5 seconds to process serially
	ecs::entity_range ents{ 0, 499, short{0} };

	// Commit the components (does not run the systems)
	ecs::commit_changes();

	// Time the serial system
	auto start = std::chrono::high_resolution_clock::now();
	serial_sys.update();
	std::chrono::duration<double> serial_time = std::chrono::high_resolution_clock::now() - start;
	std::cout << "serial system took " << serial_time.count() << " seconds\n";

	// Time the parallel system
	start = std::chrono::high_resolution_clock::now();
	parallel_sys.update();
	std::chrono::duration<double> parallel_time = std::chrono::high_resolution_clock::now() - start;
	std::cout << "parallel system took " << parallel_time.count() << " seconds\n";
}

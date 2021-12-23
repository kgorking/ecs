#include <ecs/ecs.h>
#include <iostream>
#include <memory_resource>
#include <string>

int main() {
	/* not currently implemented
	ecs::runtime ecs;

	// Set up a buffer on the stack for storage of 'std::pmr::string's
	constexpr size_t buf_size = 16384;
	char buffer[buf_size]{};
	std::pmr::monotonic_buffer_resource mono_resource(&buffer[0], buf_size);
	ecs.set_memory_resource<std::pmr::string>(&mono_resource);

	// Systems that prints out the distance in bytes from the greeting to the string data
	auto &std_sys = ecs.make_system<ecs::opts::not_parallel>([](std::string const &str) {
		std::cout << std::distance(reinterpret_cast<char const *>(&str), reinterpret_cast<char const *>(str.data())) << ' ';
	});
	auto &pmr_sys = ecs.make_system<ecs::opts::not_parallel>([](std::pmr::string const &str) {
		std::cout << std::distance(reinterpret_cast<char const *>(&str), reinterpret_cast<char const *>(str.data())) << ' ';
	});

	// Add some components
	constexpr auto sz = "some kind of semi large string";
	ecs.add_component({0, 3}, std::string{sz});
	ecs.add_component({0, 3}, std::pmr::string{sz});
	ecs.commit_changes();

	// Run the two systems
	std::cout << "Distance from std::string to string data, in bytes\n";
	std_sys.run();

	std::cout << "\n\nDistance from std::pmr::string to string data, in bytes\n";
	pmr_sys.run();

	// The buffer is about to go out of scope, so restore the default resource
	ecs.reset_memory_resource<std::pmr::string>();*/
}

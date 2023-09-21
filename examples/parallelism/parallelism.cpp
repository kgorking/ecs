#include <ecs/ecs.h>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
	using namespace std::chrono_literals;

	// The lambda used by both the serial- and parallel systems
	auto constexpr sys_sleep = [](short const &) { std::this_thread::sleep_for(50ms); };

	using namespace ecs::opts;

	// Make the systems
	ecs::runtime rt;
	auto &serial_sys = rt.make_system<not_parallel, manual_update>(sys_sleep);
	auto &parallel_sys = rt.make_system<manual_update>(sys_sleep);

	// Create a range of entities that would
	// take 5 seconds to process serially
	rt.add_component({0, 20 - 1}, short{0});

	// Commit the components (does not run the systems)
	rt.commit_changes();

	// Time the serial system
	std::cout << "Running serial system: ";
	auto start = std::chrono::high_resolution_clock::now();
	serial_sys.run();
	std::chrono::duration<double> const serial_time = std::chrono::high_resolution_clock::now() - start;
	std::cout << serial_time.count() << " seconds\n";

	// Time the parallel system
	std::cout << "Running parallel system: ";
	start = std::chrono::high_resolution_clock::now();
	parallel_sys.run();
	std::chrono::duration<double> const parallel_time = std::chrono::high_resolution_clock::now() - start;
	std::cout << parallel_time.count() << " seconds\n";
}

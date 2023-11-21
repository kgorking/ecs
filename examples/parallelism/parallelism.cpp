#include <ecs/ecs.h>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
	using namespace std::chrono_literals;
	using namespace ecs::opts;

	// The lambda used by both the serial- and parallel systems
	auto constexpr sys_sleep = [](short) { std::this_thread::sleep_for(1s); };

    // print thread-count
	std::cout << "hardware threads: " << std::thread::hardware_concurrency() << "\n\n";

	// Create a range of entities
	ecs::runtime rt;
	rt.add_component({0, 2}, short{});

	// Commit the components (does not run the systems)
	rt.commit_changes();

	// Make the systems
	auto &serial_sys = rt.make_system<not_parallel, manual_update>(sys_sleep);
	auto &parallel_sys = rt.make_system<manual_update>(sys_sleep);

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

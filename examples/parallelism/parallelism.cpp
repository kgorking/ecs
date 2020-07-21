#include <chrono>
#include <ecs/ecs.h>
#include <iostream>
#include <thread>


int main() {
    using namespace std::chrono_literals;

    // The lambda used by both the serial- and parallel systems
    auto constexpr sys_sleep = [](short const&) { std::this_thread::sleep_for(10ms); };

    // Make the systems
    auto& serial_sys = ecs::make_system(sys_sleep);
    auto& parallel_sys = ecs::make_parallel_system(sys_sleep);

    // Create a range of entities that would
    // take 5 seconds to process serially
    ecs::add_component({0, 500 - 1}, short{0});

    // Commit the components (does not run the systems)
    ecs::commit_changes();

    // Time the serial system
    std::cout << "Running serial system: ";
    auto start = std::chrono::high_resolution_clock::now();
    serial_sys.update();
    std::chrono::duration<double> const serial_time = std::chrono::high_resolution_clock::now() - start;
    std::cout << serial_time.count() << " seconds\n";

    // Time the parallel system
    std::cout << "Running parallel system: ";
    start = std::chrono::high_resolution_clock::now();
    parallel_sys.update();
    std::chrono::duration<double> const parallel_time = std::chrono::high_resolution_clock::now() - start;
    std::cout << parallel_time.count() << " seconds\n";
}

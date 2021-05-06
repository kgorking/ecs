#include <iostream>
#include <string>
#include <memory_resource>
#include <ecs/ecs.h>

// The standard component
struct greeting {
	std::string msg;
};

// The pmr allocator aware component
struct pmr_greeting {
    // use the pmr-aware string
    std::pmr::string msg;

    // Constructor that takes a c-string
	explicit pmr_greeting(char const *sz) : msg(sz) {}

    // Use helper macro to enable pmr support and declare defaults
	ECS_USE_PMR(pmr_greeting);

    // Implement required constructors for pmr support
    explicit pmr_greeting(allocator_type alloc) noexcept : msg{alloc} {}
    pmr_greeting(pmr_greeting const &g, allocator_type alloc = {}) : msg{g.msg, alloc} {}
	pmr_greeting(pmr_greeting &&g, allocator_type alloc) : msg{std::move(g.msg), alloc} {}
};


int main() {
    // Set up a buffer on the stack for storage of 'pmr_greeting's
    constexpr size_t buf_size = 16384;
    char buffer[buf_size]{};
    std::pmr::monotonic_buffer_resource mono_resource(&buffer[0], buf_size);
	ecs::set_memory_resource<pmr_greeting>(&mono_resource);


    // Systems that prints out the distance in bytes from the greeting to the string data
    auto &std_sys = ecs::make_system<ecs::opts::not_parallel>([](greeting const& g) {
		std::cout << std::distance(reinterpret_cast<char const*>(&g), reinterpret_cast<char const*>(g.msg.data())) << ' ';
    });
    auto &pmr_sys = ecs::make_system<ecs::opts::not_parallel>([](pmr_greeting const& g) {
		std::cout << std::distance(reinterpret_cast<char const*>(&g), reinterpret_cast<char const*>(g.msg.data())) << ' ';
    });


    // Add some components
    ecs::add_component({0, 3}, greeting{"some kind of semi large string"});
    ecs::add_component({0, 3}, pmr_greeting{"some kind of semi large string"});
	ecs::commit_changes();


    // Run the two systems
	std::cout << "Distance from greeting to string data, in bytes\n";
	std_sys.run();

	std::cout << "\n\nDistance from pmr_greeting to string data, in bytes\n";
	pmr_sys.run();


    // The buffer is about to go out of scope, so restore the default resource
	ecs::set_memory_resource<pmr_greeting>(std::pmr::get_default_resource());
}

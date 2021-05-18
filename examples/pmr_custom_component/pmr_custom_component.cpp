#include <iostream>
#include <string>
#include <memory_resource>
#include <ecs/ecs.h>

//
// Shows how to create a struct that is allocator aware
// 


// The pmr allocator aware component
struct pmr_greeting {
    // Use helper macro to enable pmr support and declare defaults
	ECS_USE_PMR(pmr_greeting);

    // use the pmr-aware string
    std::pmr::string msg;

    // Constructor that takes a c-string.
	explicit pmr_greeting(char const *sz) : msg(sz) {}

    //
    // Required PMR constructors
    // 

    // These are called from within other stl types, like std::pmr::vector, and pass the allocator along to the pmr::string.
    // 'allocator_type' is declared by ECS_USE_PMR().
    explicit pmr_greeting(allocator_type alloc) : msg{alloc} {}
    pmr_greeting(pmr_greeting const &g, allocator_type alloc) : msg{g.msg, alloc} {}
	pmr_greeting(pmr_greeting &&g, allocator_type alloc) : msg{std::move(g.msg), alloc} {}
};


int main() {
	ecs::runtime ecs;

    // Set up a buffer on the stack for storage of 'pmr_greeting's
    constexpr size_t buf_size = 16384;
    char buffer[buf_size]{};
    std::pmr::monotonic_buffer_resource mono_resource(&buffer[0], buf_size);
	ecs.set_memory_resource<pmr_greeting>(&mono_resource);


    // System that prints out the distance in bytes from the greeting to the string data
    auto &pmr_sys = ecs.make_system<ecs::opts::not_parallel>([](pmr_greeting const& g) {
		std::cout << std::distance(reinterpret_cast<char const*>(&g), reinterpret_cast<char const*>(g.msg.data())) << ' ';
    });


    // Add some components.
    ecs.add_component({0, 3}, pmr_greeting{"some kind of semi large string"});
	ecs.commit_changes();


    // Run the system
	std::cout << "Distance from pmr_greeting to string data, in bytes\n";
	pmr_sys.run();
	std::cout << '\n';

    // The buffer is about to go out of scope, so restore the default resource
	ecs.reset_memory_resource<pmr_greeting>();
}

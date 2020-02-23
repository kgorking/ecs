
# ecs - An entity/component/system project.
**ecs** is a header-only c++17 implementation, with focus on ease of use and speed.

# Examples
See 'examples/example' for the code.

### 1. A simple example
```cpp
#include <iostream>
#include <string>
#include <ecs/ecs.h>

int main()
{
	// A system that operates on entities with 'int' components
	ecs::make_system([](int const& i) {
	    std::cout << i << '\n';
	});
	
	// Set up 3 entities with their components
	// This uses the entity_range class, which is a
	// wrapper for the interface to allow easy usage
	ecs::entity_range more_ents{ 0, 2, int{1} };

	// Commit the changes and run the systems
	ecs::update_systems();
}
```
Running this code will print out
```
1
1
1
```

### 2. Using a lambda to initialize components
At the end of the previous main I can add the following code to add another 3 components to 3 other entities, and have a lambda produce each component for the entities
```cpp
ecs::entity_range ents{ 3, 5, [](ecs::entity_id ent) -> int { return ent.id * 2; } };
ecs::update_systems();
```
Running the code now will also print out
```
...
6
8
10
```

### 3. Adding a second component
```cpp
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
```
Adding this code and running it will print out the following, because both systems now match the entities
```
...
6: jon
8: sean
10: jimmy
```

### 4. Removing a component
Now lets remove a component and see what happens
```cpp
// Remove the integer component from the 'sean' entity using the 'ecs::entity' helper class
ecs::entity sean{ 4 };
sean.remove<int>();		// same as ecs::remove_component<int>(4);

// Commit the changes and run the systems
ecs::update_systems();
```
Running the code now will print out the following
```
...
6: jon
10: jimmy
```

### 5. Accessing the entity id
If you need to access the entity id, add either an
[ecs::entity_id](https://github.com/monkey-g/ecs/blob/master/ecs/types.h) or an [ecs::entity](https://github.com/monkey-g/ecs/blob/master/ecs/entity.h)
as the first argument in the lambda.
```cpp
ecs::make_system([](ecs::entity_id ent, std::string const& s) {
	std::cout << "entity with id " << ent.id << " is named " << s << '\n';
});
ecs::update_systems();
```
Running this will print out
```
...
entity with id 3 is named jon
entity with id 4 is named sean
entity with id 5 is named jimmy
```

### 6. Parallelism
Systems can process the components of entities in parallel, by using ```ecs::add_system_parallel```.
```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <ecs/ecs.h>

using namespace std::chrono_literals;

int main()
{
	// The lambda used by both the serial- and parallel systems
	auto constexpr sys_sleep = [](short const&) {
		std::this_thread::sleep_for(10ms);
	};

	// Add the systems
	auto& serial_sys   = ecs::make_system(sys_sleep);
	auto& parallel_sys = ecs::make_parallel_system(sys_sleep);

	// Create a range of 500 entites that would
	// take 5 seconds to process serially
	ecs::entity_range ents{ 0, 499, short{0} };

	// Commit the components to the entities (does not run the systems)
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
```
Running this on my machine prints out
```
serial system took 5.2987 seconds
parallel system took 0.766407 seconds
```

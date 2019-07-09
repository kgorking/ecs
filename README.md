
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
}
```
Running this code will print out
```
4
8
12
```

### 2. Adding a second component
At the end of the previous main I can add the following code
```cpp
// Add another system that operates on entities with 'int' and 'std::string' components
ecs::add_system([](int const& i, std::string const& s) {
    std::cout << i << ": " << s << '\n';
});

// Add a second component to the entities
jon.add(std::string{"jon"});
sean.add(std::string{"sean"});
jimmy.add(std::string{"jimmy"});

// Commit the changes and run the systems
ecs::update_systems();
```
Adding this code and running it will print out the following, because both systems now match the entities
```
4
8
12
4: jon
8: sean
12: jimmy
```

### 3. Removing a component
Now lets remove a component and see what happens
```cpp
// Remove the integer component from the 'sean' entity
sean.remove<int>();

// Commit the changes and run the systems
ecs::update_systems();
```
Running the code now will print out the following
```
4
12
4: jon
12: jimmy
```

### 4. Accessing the entity id
If you need to access the entity id, it's as easy as adding either an
[ecs::entity_id](https://github.com/monkey-g/ecs/blob/master/ecs/types.h) or an [ecs::entity](https://github.com/monkey-g/ecs/blob/master/ecs/entity.h)
as the first argument in the lambda.
```cpp
ecs::add_system([](ecs::entity_id ent, std::string const& s) {
	std::cout << "entity with id " << ent.id << " is named " << s << '\n';
	});
ecs::update_systems();
```


### 5. Parallelism
Systems can process the components of entities in parallel, simply by marking the system as being parallel.
```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <ecs/ecs.h>
#include <ecs/entity_range.h>

using namespace std::chrono_literals;

int main()
{
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


# ecs - An entity/component/system project.
**ecs** is a header-only c++17 implementation, with focus on ease of use and speed.

# Examples
### 1. A simple example
```cpp
#include <iostream>
#include <string>
#include <ecs/ecs.h>

int main()
{
	// A system that operates on entities that hold an 'int'
	ecs::add_system([](int const& i) {
	    std::cout << i << " ";
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
4 8 12
```

### 2. Adding a second component
At the end of the previous main I can add the following code
```cpp
// Add another system that operates on entities that hold an 'int' and 'std::string' (order is irrelevant)
ecs::add_system([](int const& i, std::string const& s) {
    std::cout << i << ": " << s << "\n";
});

// Add a second component to the entities
jon.add(std::string{"jon"});
sean.add(std::string{"sean"});
jimmy.add(std::string{"jimmy"});

// Commit the changes and run the systems
ecs::update_systems();
```
Adding this code and running it will print out the following, because both systems are run
```
4 8 12
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
4 12
4: jon
12: jimmy
```

# ecs - An entity/component/system project.
**ecs** is a header-only c++17 implementation, with focus on ease of use and speed.

# Examples
### A simple example
```cpp
#include <iostream>
#include <ecs/ecs.h>

int main()
{
	// A system that operates on entities that hold an 'int'
	ecs::add_system([](int const& i) {
	    std::cout << i << " ";
	});
	
	// Set up some entities with their ids and components
	ecs::entity
		a{ 0, 4 },
		b{ 1, 8 },
		c{ 2, 12 };

	// Commit the changes and run the systems
	ecs::update_systems();
}
```
Running this code will print out
> 4 8 12

### Another example with 2 components
```cpp
#include <iostream>
#include <string>
#include <ecs/ecs.h>

int main()
{
	// A system that operates on entities that hold an 'int' and 'std::string' (order is irrelevant)
	ecs::add_system([](int const& i, std::string const& s) {
	    std::cout << i << ": " << s << "\n";
	});
	
	// Set up some entities with their ids and components
	ecs::entity
		jon{ 0, 4, std::string{"jon"},
		sean{ 1, 8, std::string{"sean"} },
		jimmy{ 2, 12, std::string{"jimmy"} };

	// Commit the changes and run the systems
	ecs::update_systems();
}
```
Running this code will print out
> 4: jon
> 8: sean
> 12: jimmy


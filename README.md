
# ECS: An entity/component/system library.
This is a small project I created to better familiarize myself with the new features of c++ 17/20, and to test out
some stuff I had been reading about [data oriented design](http://www.dataorienteddesign.com/dodbook/).
An ecs library seemed like a good fit for both objectives.

## What is 'ecs'?
I won't go into to much detail here, there are plenty of resources [available](http://gameprogrammingpatterns.com/component.html)
[online](https://github.com/EngineArchitectureClub/TalkSlides/blob/master/2012/05-Components-SeanMiddleditch/ComponentDesign.pdf),
but the tl;dr is that regular classes are split into components and systems instead of being bundled together.
Components hold the data, systems implement the logic that operates on components, and entities holds components.
This allows for very efficient storage and processing, and for dynamic composition of entities at runtime.

### A minimal example
The following example shows ...
```cpp
#include <iostream>
#include <ecs/ecs.h>

// The component
struct greeting {
    char const* msg = "hello";
};

int main()
{
    // The system
    ecs::make_system([](greeting const& g) {
        std::cout << g.msg << ' ';
    });

    // The entities
    ecs::add_components({0, 100}, greeting{});

    // Run the whole thing
    ecs::update_systems();
}
```

#### The component
 * flags
#### The system
 * parallel
#### The entities
 * ranges

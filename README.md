
# ECS: An entity/component/system library.
This is a small project I created to better familiarize myself with the features of c++ 17/20, and to test out
some stuff I had been reading about [data oriented design](http://www.dataorienteddesign.com/dodbook/).
An ecs library seemed like a good fit for both objectives.

More detail on what ecs is can be found [here](http://gameprogrammingpatterns.com/component.html) and
[here](https://github.com/EngineArchitectureClub/TalkSlides/blob/master/2012/05-Components-SeanMiddleditch/ComponentDesign.pdf).

## An example
The following example shows all you need to get ecs working with this library.

```cpp
#include <ecs/ecs.h>
#include <iostream>

// The component
struct greeting {
    char const* msg = "alright";
};

int main()
{
    // The system
    ecs::make_system([](greeting const& g) {
        std::cout << g.msg << ' ';
    });

    // The entities
    ecs::add_components({0, 2}, greeting{});

    // Run it
    ecs::update_systems();
}
```
This is pretty basic, but there are plenty of ways to extend this example to do cooler things, as explained below.

## Components
There are very few restrictions on what a component can be. It does have to obey the requirements of
[std::copyable](https://en.cppreference.com/w/cpp/concepts/copyable). In the example above you could
have used a `std::string` instead of creating a component. <br>
Accessing components from systems is done through *references*. If you forget to do so, you will get a compile
time error to remind you. Remember to mark components you don't intend to change in a system as a `const` reference.

#### Component flags
The behaviour of components can be changed using component flags, which changes how they are managed
internally and can offer performance and memory benefits. There are four supported flags currently supported:

### `tag`
Marking a component as a tag is used for components that signal some kind of state, without needing to
take up any memory. For instance, you could use it tag certain entities as having some form of capability,
like a 'freezable' tag to mark stuff that can be frozen.
```cpp
struct freezable {
    ecs_flags(ecs::tag);
};
```
Using tagged components in systems has a slightly different syntax to regular components, namely that they are
passed by value. This is to discourage the use of tags to share some kind of data, which you should use the `share` flag
for instead (see below)
```cpp
ecs::make_system([](greeting const& g, freezable) {
  // code to operate on entities that has a greeting and are freezable
});
```
If tag components are marked as anything other than pass-by-value, the compiler will drop a little error to remind you.

### `share`


### `transient`


### `immutable`


## Systems
* rules
* multi-component systems
* entity reference
* requirements
* parallel

## Entities
* ranges
* component generators

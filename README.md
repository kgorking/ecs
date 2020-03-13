
# ECS: An entity/component/system library.
This is a small project I created to better familiarize myself with the features of c++ 17/20, and to test out
some stuff I had been reading about [data oriented design](http://www.dataorienteddesign.com/dodbook/).
An ecs library seemed like a good fit for both objectives.

More detail on what ecs is can be found [here](http://gameprogrammingpatterns.com/component.html) and
[here](https://github.com/EngineArchitectureClub/TalkSlides/blob/master/2012/05-Components-SeanMiddleditch/ComponentDesign.pdf).

# Table of Contents

* [An example](#An-example)
* [Components](#Components)
  * [Generators](#Generators)
  * [Flags](#Flags)
    * [`tag`](#tag)
    * [`share`](#share)
    * [`transient`](#transient)
    * [`immutable`](#immutable)
* [Systems](#Systems)
  * [Requirements and rules](#Requirements-and-rules)
  * [Multi-component systems](#Multi-component-systems)
  * [Current entity](#Current-entity)
  * [Parallel systems](#Parallel-systems)
* [Entities](#Entities)
  * [Ranges](#Ranges)

# An example
The following example shows all you need to get started.

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
Running this will do a Matthew McConaughey impression and print 'alright alright alright '.

This is pretty basic, but there are plenty of ways to extend this example to do cooler things, as explained below.

# Components
There are very few restrictions on what a component can be. It does have to obey the requirements of
[std::copyable](https://en.cppreference.com/w/cpp/concepts/copyable) though. In the example above you could
have used a `std::string` instead of creating a custom component, and it would work just fine.

Accessing components from systems is done through *references*. If you forget to do so, you will get a compile-time
error to remind you. Remember to mark components you don't intend to change in a system as a `const` reference.

## Generators
When adding components to entities, you can specify a generator instead of a default constructed component
if you need the individual components to have different initial states. Generators must have the signature
of `T(ecs::entity_id)`, where `T` is the component type that the generator makes.
In the [mandelbrot](https://github.com/kgorking/ecs/blob/master/examples/mandelbrot/mandelbrot.cpp) example,
a generator is used to create the (x,y) coordinates of the individual pixels from the entity id:
```cpp
constexpr int dimension = 500;
struct pos {
    int x, y;
};

// ...

ecs::add_components({ 0, dimension * dimension},
    [](ecs::entity_id ent) -> pos {
        int const x = ent % dimension;
        int const y = ent / dimension;
        return { x, y };
    }
);
```

## Flags
The behaviour of components can be changed using component flags, which changes how they are managed
internally and can offer performance and memory benefits. There are four supported flags currently supported:

### `tag`
Marking a component as a tag is used for components that signal some kind of state, without needing to
take up any memory. For instance, you could use it tag certain entities as having some form of capability,
like a 'freezable' tag to mark stuff that can be frozen.

```cpp
struct freezable { ecs_flags(ecs::tag);
};
```

Using tagged components in systems has a slightly different syntax to regular components, namely that they are
passed by value. This is to discourage the use of tags to share some kind of data, which you should use the `share` flag
for instead (see below).

```cpp
ecs::make_system([](greeting const& g, freezable) {
  // code to operate on entities that has a greeting and are freezable
});
```

If tag components are marked as anything other than pass-by-value, the compiler will drop a little error message to remind you.

**Note** This flag is mutually exclusive with `share`.

### `share`
Marking a component as shared is used for components that hold data that is shared between all entities the component is added to.

```cpp
struct frame_data { ecs_flags(ecs::share);
    double delta_time = 0.0;
};
// ...
ecs::make_system([](position& pos, velocity const& vel, frame_data const& fd) {
    pos += vel * fd.delta_time;
});
```

**Note!** Beware of using mutable shared components in parallel systems, as it can lead to race conditions. Combine it with `immutable`, if possible,
to disallow systems modifying the shared component. `ecs_flags(ecs::share|ecs::immutable);`

### `transient`
Marking a component as transient is used for components that only exists on entities temporarily. The runtime will remove these components
from entities automatically after one cycle.
```cpp
struct damage { ecs_flags(ecs::transient);
    double value;
};
// ...
ecs::make_system([](health& h, damage const& dmg) {
    h.hp -= dmg.value;
});
```
After the next call to `ecs::commit_changes()` all `damage` components on all entities will have been removed.

### `immutable`
Marking a component as immutable (a.k.a. const) is used for components that are not to be changed by systems.
This is used for passing read-only data to systems. A great candiate for this would be the `frame_data` component
from the `share` example.

If a component is marked as `immutable` and is used in a system without being marked `const`,
you will get a compile-time error reminding you to make it constant.

# Systems
## Requirements and rules
## Multi-component systems
## Current entity
## Parallel systems

# Entities
## ranges
## component generators

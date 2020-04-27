
# ECS: An entity/component/system library.
This is a small project I created to better familiarize myself with the features of c++ 17/20, and to test out
some stuff I had been reading about [data oriented design](http://www.dataorienteddesign.com/dodbook/).
An ecs library seemed like a good fit for both objectives.

More detail on what ecs is can be found [here](http://gameprogrammingpatterns.com/component.html) and
[here](https://github.com/EngineArchitectureClub/TalkSlides/blob/master/2012/05-Components-SeanMiddleditch/ComponentDesign.pdf).

# An example
The following example shows the basics of the library.

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

# Table of Contents
* [Entities](#Entities)
* [Components](#Components)
  * [Adding components to entities](#Adding-components-to-entities)
  * [Committing component changes](#Committing-component-changes)
  * [Generators](#Generators)
  * [Flags](#Flags)
    * [`tag`](#tag)
    * [`share`](#share)
    * [`transient`](#transient)
    * [`immutable`](#immutable)
* [Systems](#Systems)
  * [Current entity](#Current-entity)
  * [Requirements and rules](#Requirements-and-rules)
  * [Parallel systems](#Parallel-systems)
  * [Groups](#Groups)


# Entities
Entities are the scaffolding on which you build your objects. There a three entity classes in the library, each offering increasingly more advanced usage.

* [`ecs::entity_id`](#https://github.com/kgorking/ecs/blob/master/include/ecs/entity_id.h) is a wrapper for an integer identifier.
* [`ecs::entity`](#https://github.com/kgorking/ecs/blob/master/include/ecs/entity.h) is a slightly more useful wrapper for `ecs::entity_id` which adds some helper-functions to ease to usage of entity-component interactions.
* [`ecs::entity_range`](#https://github.com/kgorking/ecs/blob/master/include/ecs/entity_range.h) is the preferred way to deal with many entities at once in a concise and efficient manner. The start- and end entity id is inclusive when passed to an entity_range, so `entity_range some_range{0, 100}` has a range of 101 entities.

The management of entity id's is left to user.

# Components
There are very few restrictions on what a component can be, but it does have to obey the requirements of
[std::copyable](https://en.cppreference.com/w/cpp/concepts/copyable). In the example above you could
have used a `std::string` instead of creating a custom component, and it would work just fine.

You can add as many components to an entity as you need; there is no upper limit.

## Adding components to entities
The simplest way to add components to entities is thought the helper-functions on `ecs::entity_range` and `ecs::entity`. Components can also be added using the free-standing functions `ecs::add_component`.

```cpp
ecs::entity ent{0};         // entity helper-class working on id 0
ent.add<int>();             // add a default-initialized integer to entity 0
ent.add(2L);                // add a long with value 2 to entity 0 (deduces type from the argument)
ent.add(4UL, 3.14f, 6.28);  // add an unsigned long, a float, and a double to entity 0
ecs::add_component(0, 6LL); // add a long long to entity 0

ecs::entity_range more_ents{1,100};  // entity helper-class working on ids from 1 to (and including) 100
more_ents.add(3, 0.1f);              // add 100 ints with value 3 and 100 floats with value 0.1f
ecs::add_component({1,50}, 6LL);     // add a long long to 50 entities
// etc..
```

## Committing component changes
All calls to add or remove components from entities are defered, and will not be processed until a call to `ecs::commit_changes()` or `ecs::update_systems()` is called, where the latter function also calls the former. Changes should only be committed once per cycle.

By defering the components changes to entites, it is possible to safely add and remove components in parallel systems, without the fear of causing data-races or doing unneeded locks.


## Generators
When adding components to entities, you can specify a generator instead of a default constructed component
if you need the individual components to have different initial states. Generators have the signature
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
The behaviour of components can be changed by using component flags, which can change how they are managed
internally and can offer performance and memory benefits. Flags can be added to components using the `ecs_flags()` macro:

### `tag`
Marking a component as a tag is used for components that signal some kind of state, without needing to
take up any memory. For instance, you could use it to tag certain entities as having some form of capability,
like a 'freezable' tag to mark stuff that can be frozen.

```cpp
struct freezable { ecs_flags(ecs::tag);
};
```

Using tagged components in systems has a slightly different syntax to regular components, namely that they are
passed by value. This is to discourage the use of tags to share some kind of data, which you should use the `share` flag
for instead.

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
to disallow systems modifying the shared component, using `ecs_flags(ecs::share|ecs::immutable);`

### `immutable`
Marking a component as immutable (a.k.a. const) is used for components that are not to be changed by systems.
This is used for passing read-only data to systems. If a component is marked as `immutable` and is used in a system without being marked `const`, you will get a compile-time error reminding you to make it constant.

### `transient`
Marking a component as transient is used for components that only exists on entities temporarily. The runtime will remove these components
from entities automatically after one cycle.
```cpp
struct damage { ecs_flags(ecs::transient);
    double value;
};
// ...
ecs::add_components({0,99}, damage{9001});
ecs::commit_changes(); // adds the 100 damage components
ecs::commit_changes(); // removes the 100 damage components
```

# Systems
Systems are where the code that operates on one or more components is located. A system is built from a
user-provided lambda using the functions `ecs::make_system` or `ecs::make_parallel_system`. 
Systems can operate on as many components as you need; there is no limit.

Accessing components from systems is done through *references*. If you forget to do so, you will get a compile-time
error to remind you. Remember to mark components you don't intend to change in a system as a `const` reference.


## The current entity
If you need access to the entity currently being processed by a system, make the first parameter type either
an `ecs::entity_id` or `ecs::entity`. The entity will only be passed as a value, so trying to accept it as anything else will result in a compile time error.

```cpp
ecs::make_system([](ecs::entity_id ent, greeting const& g) {
    std::cout << "entity with id " << ent << " says: " << g.msg << '\n';
});
```

## Requirements and rules
There are a few requirements and restrictions put on the provided lambdas to ensure that the runtime
works as expected and the internal state makes sense.

* **No return values.** Systems are not permitted to have return values, because it logically does not make any sense.
  Systems with return types other than `void` will result in a compile time error.
* **At least one component parameter.** Systems operate on components, so if none is provided it will result in a compile time error.
* **No duplicate components.** Having the same component more than once in the parameter list is likely an error on
  the programmers side, so a compile time error will be raised if it occurs. 

## Parallel systems
Parallel systems can offer great speed-ups on multi-core machines, if the system in question has enough work to merit it. There is always some overhead associated with running code in multiple threads, and if the systems can not supply enough work for the threads you will end up loosing performance instead. A good profiler can often help with this determination.

The dangers of multi-threaded code also exist in parallel systems, so take the same precautions here as you would in regular parallel code.

## Groups
Systems can be roughly segmented in to groups by passing along a compile-time integer as a template paramater to `ecs::make_system`. Systems are executed in the order they are made, but are stable-sorted on their group id. Systems with no group id specified are put in group 0.

```cpp
ecs::make_system<1>([](int&) {
    std::cout << "hello from group one\n";
});
ecs::make_system<-1>([](int&) {
    std::cout << "hello from group negative one\n";
});
ecs::make_system([](int&) {
    std::cout << "hello from group whatever\n";
});
// ...
ecs::update_systems();
```
Running the above code will print out
> hello from group negative one\
> hello from group whatever\
> hello from group one


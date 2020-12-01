
# ECS: An entity/component/system library.
This is a small project I created to better familiarize myself with the features of c++ 17/20, and to test out
some stuff I had been reading about [data oriented design](http://www.dataorienteddesign.com/dodbook/).
An ecs library seemed like a good fit for both objectives.

More detail on what ecs is can be found [here](http://gameprogrammingpatterns.com/component.html) and
[here](https://github.com/EngineArchitectureClub/TalkSlides/blob/master/2012/05-Components-SeanMiddleditch/ComponentDesign.pdf).

# An example
The following example shows the basics of the library.

```cpp
#include <iostream>
#include <ecs/ecs.h>

// The component
struct greeting {
    char const* msg;
};

int main() {
    // The system
    ecs::make_system([](greeting const& g) {
        std::cout << g.msg;
    });

    // The entities
    ecs::add_component({0, 2}, greeting{"alright "});

    // Run it
    ecs::update();
}
```
Running this will do a Matthew McConaughey impression and print 'alright alright alright '.
This is a fairly simplistic sample, but there are plenty of ways to extend it to do cooler things.

Use this [Compiler explorer test link](https://godbolt.org/z/jE6MTq) to try it out for yourself, using the [single-include header](include/ecs/ecs_sh.h).


# Building 
#### Tested compilers
The CI build status for msvc, clang 10, and gcc 10.1 is currently:
* ![msvc 19.26](https://github.com/kgorking/ecs/workflows/msvc%2019.26/badge.svg?branch=split_ci_builds)
* ![gcc 10.1](https://github.com/kgorking/ecs/workflows/gcc%2010.1/badge.svg?branch=split_ci_builds)
* ![clang 10 ms-stl](https://github.com/kgorking/ecs/workflows/clang%2010%20ms-stl/badge.svg?branch=split_ci_builds)
* ![clang 10 libc++](https://github.com/kgorking/ecs/workflows/clang%2010%20libc++/badge.svg?branch=split_ci_builds)
  * `libc++` is missing `<concepts>` and the parallel stl implementation.
* ![clang 10 libstdc++](https://github.com/kgorking/ecs/workflows/clang%2010%20libstdc++/badge.svg?branch=split_ci_builds)
  * `libstdc++` is missing the `<span>` header.

# Table of Contents
- [Entities](#entities)
- [Components](#components)
  - [Adding components to entities](#adding-components-to-entities)
  - [Committing component changes](#committing-component-changes)
  - [Generators](#generators)
  - [Flags](#flags)
    - [`tag`](#tag)
    - [`share`](#share)
    - [`immutable`](#immutable)
    - [`transient`](#transient)
    - [`global`](#global)
- [Systems](#systems)
  - [Requirements and rules](#requirements-and-rules)
  - [The current entity](#the-current-entity)
  - [Sorting](#sorting)
  - [Filtering](#filtering)
  - [Hierarchies](#hierarchies)
  - [Parallel-by-default systems](#parallel-by-default-systems)
  - [Automatic concurrency](#automatic-concurrency)
  - [Options](#options)
    - [`opts::frequency<hz>`](#optsfrequencyhz)
    - [`opts::group<group number>`](#optsgroupgroup-number)
    - [`opts::manual_update`](#optsmanual_update)
    - [`opts::not_parallel`](#optsnot_parallel)


# Entities
Entities are the scaffolding on which you build your objects, and there are two classes in the library for managing entities.

* [`ecs::entity_id`](https://github.com/kgorking/ecs/blob/master/include/ecs/entity_id.h) is a wrapper for an integer identifier.
* [`ecs::entity_range`](https://github.com/kgorking/ecs/blob/master/include/ecs/entity_range.h) is the preferred way to deal with many entities at once in a concise and efficient manner. The start- and end entity id is inclusive when passed to an entity_range, so `entity_range some_range{0, 100}` will span 101 entities.

The management of entity id's is left to user.

# Components
Components hold the data and are added to entities. There are very few restrictions on what components can be, but they do have to obey the requirements of [CopyConstructible](https://en.cppreference.com/w/cpp/named_req/CopyConstructible). In the example above you could have used a `std::string` instead of creating a custom component, and it would work just fine.

You can add as many different components to an entity as you need; there is no upper limit. You can not add more than one of the same type.

## Adding components to entities
Adding components is done with the function `ecs::add_component()`.

```cpp
ecs::add_component(0, 4UL, 3.14f, 6.28); // add an unsigned long, a float, and a double to entity 0

ecs::entity_id ent{1};
ecs::add_component(ent, "hello");        // add const char* to entity 1

ecs::entity_range more_ents{1,100};      // entity range of ids from 1 to (and including) 100
ecs::add_component(more_ents, 3, 0.1f);  // add 100 ints with value 3 and 100 floats with value 0.1f
ecs::add_component({1,50}, 'A', 2.2);    // add a char and a double to 50 entities
// etc..
```

## Committing component changes
Adding and removing components from entities are deferred, and will not be processed until a call to `ecs::commit_changes()` or `ecs::update()` is called, where the latter function also calls the former. Changes should only be committed once per cycle.

By deferring the components changes to entities, it is possible to safely add and remove components in parallel systems, without the fear of causing data-races or doing unneeded locks.


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

ecs::add_component({ 0, dimension * dimension},
    [](ecs::entity_id ent) -> pos {
        int const x = ent % dimension;
        int const y = ent / dimension;
        return { x, y };
    }
);
```

## Flags
The behavior of components can be changed by using component flags, which can change how they are managed
internally and can offer performance and memory benefits. Flags can be added to components using the `ecs_flags()` macro:

### `tag`
Marking a component as a *tag* is used for components that signal some kind of state, without needing to
take up any memory. For instance, you could use it to tag certain entities as having some form of capability,
like a 'freezable' tag to mark stuff that can be frozen.

```cpp
struct freezable {
    ecs_flags(ecs::flag::tag);
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
Marking a component as *shared* is used for components that hold data that is shared between all entities the component is added to.

```cpp
struct frame_data {
    ecs_flags(ecs::flag::share);
    double delta_time = 0.0;
};
// ...
ecs::add_component({0, 100}, position{}, velocity{}, frame_data{});
// ...
ecs::make_system([](position& pos, velocity const& vel, frame_data const& fd) {
    pos += vel * fd.delta_time;
});
```

**Note!** Beware of using mutable shared components in parallel systems, as it can lead to race conditions. Combine it with `immutable`, if possible,
to disallow systems modifying the shared component, using `ecs_flags(ecs::flag::share, ecs::flag::immutable);`

### `immutable`
Marking a component as *immutable* (a.k.a. const) is used for components that are not to be changed by systems.
This is used for passing read-only data to systems. If a component is marked as `immutable` and is used in a system without being marked `const`, you will get a compile-time error reminding you to make it constant.

### `transient`
Marking a component as *transient* is used for components that only exists on entities temporarily. The runtime will remove these components
from entities automatically after one cycle.
```cpp
struct damage {
    ecs_flags(ecs::flag::transient);
    double value;
};
// ...
ecs::add_component({0,99}, damage{9001});
ecs::commit_changes(); // adds the 100 damage components
ecs::commit_changes(); // removes the 100 damage components
```

### `global`
Marking a component as *global* is used for components that hold data that is shared between all systems the component is added to, without the need to explicitly add the component to any entity. Adding global components to entities is not possible.

```cpp
struct frame_data {
    ecs_flags(ecs::flag::global);
    double delta_time = 0.0;
};
// ...
ecs::add_component({0, 100}, position{}, velocity{});
// ...
ecs::make_system([](position& pos, velocity const& vel, frame_data const& fd) {
    pos += vel * fd.delta_time;
});
```

# Systems
Systems are where the code that operates on an entities components is located. A system is built from a user-provided lambda using the function `ecs::make_system`. Systems can operate on as many components as you need; there is no limit.

Accessing components in systems is done through *references*. If you forget to do so, you will get a compile-time error to remind you.

Remember to mark components you don't intend to change in a system as `const`, as this will help the sceduler by allowing the system to run concurrently with other systems that also only reads from the component. There is more information available in the [automatic concurrency](#Automatic-concurrency) section.


## Requirements and rules
There are a few requirements and restrictions put on the lambdas:

* **No return values.** Systems are not permitted to have return values, because it logically does not make any sense. Systems with return types other than `void` will result in a compile time error.
* **At least one component parameter.** Systems operate on components, so if none is provided it will result in a compile time error.
* **No duplicate components.** Having the same component more than once in the parameter list is likely an error on the programmers side, so a compile time error will be raised. 


## The current entity
If you need access to the entity currently being processed by a system, make the first parameter type an `ecs::entity_id`. The entity will only be passed as a value, so trying to accept it as anything else will result in a compile time error.

```cpp
ecs::make_system([](ecs::entity_id ent, greeting const& g) {
    std::cout << "entity with id " << ent << " says: " << g.msg << '\n';
});
```


## Sorting
An additional function object can be passed along to `ecs::make_system` to specify the order in which components are processed. It must adhere to the [*Compare*](https://en.cppreference.com/w/cpp/named_req/Compare) requirements.

```cpp
// sort ascending
auto &sys_dec = ecs::make_system(
    [](int const&) { /* ... */ },
    std::less<int>());

// sort descending
auto & sys_asc = ecs::make_system(
    [](int const&) { /* ... */ },
    std::greater<int>());

// sort length
auto &sys_pos = ecs::make_system(
    [](position& pos, some_component const&) { /* ... */ },
    [](position const& p1, position const& p2) { return p1.length() < p2.length(); });
```

* Integers passed to `sys_dec` will arrive in descending order, from highest to lowest.
* Integers passed to `sys_asc` will arrive in ascending order.
* Positions passed to `sys_pos` will be sorted according to their length. You could also have sorted on the `some_component`.

Sorting functions must correspond to a type that is processed by the system, or an error will be raised during compilation.

**Note** Adding a sorting function takes up additional memory to maintain the sorted state, and it might adversely affect cache efficiency. Only use it if necessary.


## Filtering
Components can be filtered by marking the component you wish to filter as a pointer argument:
```cpp
ecs::make_system([](int&, float*) { /* ... */ });
```
This system will run on all entities that has an `int` component and no `float` component.

More than one filter can be present; there is no limit.

**Note** `nullptr` is always passed to filtered components, so don't try to read from them.


## Hierarchies
Hierarchies can be created by adding the special component `ecs::parent` to an entity:
```cpp
add_component({1}, parent{0});
```
This alone does not create a hierarchy as such, but it makes it possible for systems to act on this relationship data. To access the parent component in a system, add a `ecs::parent<>` parameter:

```cpp
make_system([](entity_id id, parent<> const& p) {
  // id == 1, p.id() == 0
});
```
The angular brackets are needed because `ecs::parent` is a templated component which allows you to specify which, if any, of the parents components you would like access to.

`ecs::parent` must always be taken as a reference in systems, or an error will be reported.

### Accessing parent components
A parents sub-components can be accessed by specifying them in a systems parent parameter. It can the be accessed through the `get<T>` function on `ecs::parent`, where `T` specifies the type you want to accesss. If `T` is not specified in the sub-components of a systems parent parameter, an error will be raised.

 More than one sub-component can be specified; there is no upper limit.
```cpp
add_component(2, short{10});
add_component(3, long{20});
add_component(4, float{30});

add_component({5, 7}, parent{2});   // short children, parent 2 has a short
add_component({8, 10}, parent{3});  // long children, parent 3 has a long
add_component({11, 13}, parent{4}); // float children, parent 4 has a float

// Systems that only runs on entities that has a parent with a specific component,
make_system([](parent<short> const& p) { /* 10 == p.get<short>() */ });  // runs on entities 5-7
make_system([](parent<long>  const& p) { /* 20 == p.get<long>() */ });   // runs on entities 8-10
make_system([](parent<float> const& p) { /* 30 == p.get<float>() */ });  // runs on entities 11-13

// Fails
//make_system([](parent<short> const& p) { p.get<int>(); });  // will not compile; no 'int' in 'p'
```

### Filtering on parents components
Filters work like regular system filters, and can be specified on a parents sub-components:
```cpp
make_system([](parent<short*> const& p) { });  // runs on entities 8-13
```

Marking the parent itself as a filter means that any entity with a parent component on it will be ignored. Any sub-components specified are ignored.
```cpp
make_system([](parent<> *p) { });  // runs on entities 2-4
```

### Traversal
Hierarchies are, by default, traversed in a depth-first order. In the future this will be configurable to fx breath-first, no-order, etc..



## Parallel-by-default systems
Parallel systems can offer great speed-ups on multi-core machines, if the system in question has enough work to merit it. There is always some overhead associated with running code in multiple threads, and if the systems can not supply enough work for the threads you will end up loosing performance instead. A good profiler can often help with this determination.

The dangers of multi-threaded code also exist in parallel systems, so take the same precautions here as you would in regular parallel code.

Adding and removing components from entities in parallel systems is a thread-safe operation.


## Automatic concurrency
Whenever a system is made, it will internally be scheduled for execution concurrently with other systems, if the systems dependencies permit it. Systems are always scheduled so they don't interfere with each other, and the order in which they are made is respected.

Dependencies are determined based on the components a system operate on.

If a component is written to, the system that previously read from or wrote to that component becomes a dependency, which means that the system must be run to completion before the new system can execute. This ensures that no data-races occur.

If a component is read from, the system that previously wrote to it becomes a dependency.

Multiple systems that read from the same component can safely run concurrently.

## Options
The following options can be passed along to `make_system` calls in order to change the behaviour of a system. If an option is added more than once, only the first option is used.

### `opts::frequency<hz>`
`opts::frequency` is used to limit the number of times per second a system will run. The number of times the system is run may be lower than the frequency passed, but it will never be higher.

```cpp
#include <chrono>
// ...
ecs::make_system<ecs::opts::frequency<10>>([](int const&) {
    std::cout << "at least 100ms has passed\n";
});
// ...
ecs::add_component(0, int{});

// Run the system for 1 second (include <chrono>)
auto const start = std::chrono::high_resolution_clock::now();
while (std::chrono::high_resolution_clock::now() - start < 1s)
    ecs::run_systems();
```


### `opts::group<group number>`
Systems can be segmented into groups by passing along `opts::group<N>`, where `N` is a compile-time integer constant, as a template parameter to `ecs::make_system`. Systems are roughly executed in the order they are made, but groups ensure absolute separation of systems. Systems with no group id specified are put in group 0.

```cpp
ecs::make_system<ecs::opts::group<1>>([](int const&) {
    std::cout << "hello from group one\n";
});
ecs::make_system<ecs::opts::group<-1>>([](int const&) {
    std::cout << "hello from group negative one\n";
});
ecs::make_system([](int const&) { // same as 'ecs::opts::group<0>'
    std::cout << "hello from group whatever\n";
});
// ...
ecs::add_component(0, int{});
ecs::update();
```

Running the above code will print out
> hello from group negative one\
> hello from group whatever\
> hello from group one

**Note:** systems from different groups are never executed concurrently, and all systems in one group will run to completion before the next group is run.


### `opts::manual_update`
Systems marked as being manually updated will not be added to scheduler, and will thus require the user to call the `system::run()` function themselves.
Calls to `ecs::commit_changes()` will still cause the system to respond to changes in components.

```cpp
auto& manual_sys = ecs::make_system<ecs::opts::manual_update>([](int const&) { /* ... */ });
// ...
ecs::add_component(0, int{});
ecs::update(); // will not run 'manual_sys'
manual_sys.run(); // required to run the system
```

### `opts::not_parallel`
This option will prevent a system from processing components in parallel, which can be beneficial when a system does little work.

It should not be used to avoid data races when writing to a shared variable. Use atomics or [`tls::splitter`](https://github.com/kgorking/tls/blob/master/include/tls/splitter.h) in these cases, if possible.

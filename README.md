
# ECS: An entity/component/system library.
This is a small project I created to better familiarize myself with the features of c++ 17/20 and to test out
some stuff I had been reading about [data oriented design](http://www.dataorienteddesign.com/dodbook/).
An ecs library seemed like a good fit for both objectives.

More detail on what ecs is can be found [here](http://gameprogrammingpatterns.com/component.html) and
[here](https://github.com/EngineArchitectureClub/TalkSlides/blob/master/2012/05-Components-SeanMiddleditch/ComponentDesign.pdf).

Topics with the <img src="https://godbolt.org/favicon.ico" width="32"> compiler-explorer logo next to them have an example that you can play around with. Ctrl/CMD+Click the icon to open it in a new window.

# An example[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/6eoxx5PTs)
The following example shows the basics of the library.

```cpp
import ecs; // or #include <ecs/ecs.h> if library is not built as a module.
#include <iostream>

// The component
struct greeting {
    char const* msg;
};

int main() {
    ecs::runtime rt;

    // The system
    rt.make_system([](greeting const& g) {
        std::cout << g.msg;
    });

    // The entities
    rt.add_component({0, 2}, greeting{"alright "});

    // Run it
    rt.update();
}
```

Running this will do a Matthew McConaughey impression and print 'alright alright alright '.
This is a fairly simplistic sample, but there are plenty of ways to extend it to do cooler things.

# Getting the Source

1. Clone this project
2. `git submodule update --init --recursive --remote`

The latter command will fetch the submodules required to build this library.

## Tested compilers
The CI build status for msvc, clang, and gcc is currently

[![MSVC](https://github.com/kgorking/ecs/actions/workflows/msvc.yml/badge.svg)](https://github.com/kgorking/ecs/actions/workflows/msvc.yml)
[![Clang](https://github.com/kgorking/ecs/actions/workflows/clang.yml/badge.svg)](https://github.com/kgorking/ecs/actions/workflows/clang.yml)
[![GCC](https://github.com/kgorking/ecs/actions/workflows/gcc.yml/badge.svg)](https://github.com/kgorking/ecs/actions/workflows/gcc.yml)


## Initial support for modules
The library can be built as a module that can be imported with `import ecs;`. You can also use the regular `#include <ecs/ecs.h>` which imports the module in the header when modules are enabled. This makes it easy to support both include- and modular builds.

Enable module building by setting the option `ECS_COMPILE_AS_MODULE` to `true` in your CMake script (`set(ECS_COMPILE_AS_MODULE ON)`) or setting it in your CMakePresets.json file.

MSVC v14.3 (2022) or clang 17+ is required to use modules. 
GCC support will be added when version 14 is eventually released.

#### Note when using clang!
Clangs (17+18) ODR detection code gives false positives when used with modules, so when including other headers, make sure they appear *after* the module import.

```cpp
// Correct, should not induce ODR errors in clang
import ecs;
#include <vector>

// Wrong, might induce ODR errors in clang
#include <vector>
import ecs;
```



# Table of Contents
- [Entities](#entities)
- [Components](#components)
  - [Adding components to entities](#adding-components-to-entities)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/E3hrxEez8)
  - [Committing component changes](#committing-component-changes)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/8sTcG9YYv)
  - [Generators](#generators)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/GoMdKobx5)
- [Systems](#systems)
  - [Requirements and rules](#requirements-and-rules)
  - [Parallel-by-default systems](#parallel-by-default-systems)
  - [Automatic concurrency](#automatic-concurrency)
  - [The current entity](#the-current-entity)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/z9xYvd4Gc)
  - [Sorting](#sorting)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/xKfTdWqnY)
  - [Filtering](#filtering)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/zE9Yh5Kbh)
  - [Hierarchies](#hierarchies)
    - [Accessing parent components](#Accessing-parent-components)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/Toxc5MTbj)
    - [Filtering on parents components](#Filtering-on-parents-components)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/v14T1efbK)
    - [Traversal and layout](#Traversal-and-layout)
- [System options](#system-options)
  - [`opts::interval<ms, us>`](#optsintervalms-us)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/aGM86KWdf)
  - [`opts::group<group number>`](#optsgroupgroup-number)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/ezoq17fbr)
  - [`opts::manual_update`](#optsmanual_update)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/TxvndcTEq)
  - [`opts::not_parallel`](#optsnot_parallel)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/MK9xcTedq)
- [Component Flags](#component-flags)
  - [`tag`](#tag)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/dj8WjTWbE)
  - [`immutable`](#immutable)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/rnbsooorb)
  - [`transient`](#transient)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/W7hvrnjT6)
  - [`global`](#global)[<img src="https://godbolt.org/favicon.ico" width="16">](https://godbolt.org/z/ETjKzbE7o)
    - [Global systems](#Global-systems)


# Entities
Entities are the scaffolding on which you build your objects. There are two classes in the library for managing entities:

* [`ecs::entity_id`](https://github.com/kgorking/ecs/blob/master/include/ecs/entity_id.h) is a wrapper for an integer identifier.
* [`ecs::entity_range`](https://github.com/kgorking/ecs/blob/master/include/ecs/entity_range.h) is the preferred way to deal with many entities at once in a concise and efficient manner. The start- and end entity id is inclusive when passed to an entity_range, so `entity_range some_range{0, 100}` will span 101 entities.

There is no such thing as creating or destroying entities in this library; all entities implicitly exists, and this library only tracks which of those entities have components attached to them.

The management of entity id's is left to user.

# Components
Components hold the data that is added to entities.

There are very few restrictions on what components can be, but they do have to obey the requirements of [CopyConstructible](https://en.cppreference.com/w/cpp/named_req/CopyConstructible). In the example above you could have used a `std::string` instead of creating a custom component, and it would work just fine.

You can add as many different components to an entity as you need; there is no upper limit. You can not add more than one of the same type.

<br>

## Adding components to entities[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/E3hrxEez8)
Adding components is done with the following functions:

### `ecs::runtime::add_component()`
Adds singular values to one-or-more entities.
```cpp
rt.add_component(0, 4UL, 3.14f, 6.28); // add an unsigned long, a float, and a double to entity 0

ecs::entity_id ent{1};
rt.add_component(ent, "hello"sv);      // add std::string_view to entity 1

ecs::entity_range more_ents{1,100};     // entity range of ids from 1 to (and including) 100
rt.add_component(more_ents, 3, 0.1f);  // add 100 ints with value 3 and 100 floats with value 0.1f
rt.add_component({1,50}, 'A', 2.2);    // add a char and a double to 50 entities
// etc..
```

### `ecs::runtime::add_component_span()`
Copies the values from a user provided span that satisfies [std::ranges::contiguous_range](https://en.cppreference.com/w/cpp/ranges/contiguous_range). The entity- and component count must be equal.
```cpp
std::vector<int> vec{ /* ... */ }
ecs::entity_range range{1, vec.size()};
rt.add_component_span(range, vec);
```

### `ecs::runtime::add_component_generator()` [<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/xefMMz93r)
Fills the components of an entity range with the result of calling a user-supplied generator function. The function must be of the format `T(ecs::entity_id)`, where `T` is the component type returned by the function. The function will be called, in order, for each entity id in the range. The component type is automatically deduced from the generators return type.

In the [mandelbrot](https://github.com/kgorking/ecs/blob/master/examples/mandelbrot/mandelbrot.cpp) example,
a generator is used to create the (x,y) coordinates of the individual pixels from the entity id:

```cpp
constexpr int dimension = 500;
struct pos {
    int x, y;
};

// ...

rt.add_component_generator({ 0, dimension * dimension},
    [](ecs::entity_id ent) -> pos {
        int const x = ent % dimension;
        int const y = ent / dimension;
        return { x, y };
    }
);
```

<br>

## Committing component changes[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/8sTcG9YYv)
Adding and removing components from entities are deferred, and will not be processed until a call to `ecs::commit_changes()` or `ecs::update()` is called, where the latter function also calls the former. Changes should only be committed once per cycle.

By deferring the components changes to entities, it is possible to safely add and remove components in parallel systems, without the fear of causing data-races or doing unneeded locks.


# Systems
Systems holds the logic that operates on components that are attached to entities, and are built using `ecs::runtime::make_system` by passing it a lambda or a free-standing function.

```cpp
#include <ecs/ecs.h>

struct component1;
struct component2;
struct component3;

void read_only_system(component1 const&) { /* logic */ }
auto read_write_system = [](component1&, component2 const&) { /* logic */ }

int main() {
    ecs::runtime rt;
    rt.make_system(read_only_system);
    rt.make_system(read_write_system);
    rt.make_system([](component2&, component3&) { // read/write to two components
        /* logic */
    });
}
```

Systems can operate on as many components as you need; there is no limit.

Accessing large components in systems should be done through references to avoid unnecessary copying of the components data.

Remember to mark components you don't intend to change in a system as `const`, as this will help the sceduler by allowing the system to run concurrently with other systems that also only reads from the component. There is more information available in the [automatic concurrency](#Automatic-concurrency) section.


## Requirements and rules
There are a few requirements and restrictions put on the lambdas:

* **No return values.** Systems are not permitted to have return values, because it logically does not make any sense. Systems with return types other than `void` will result in a compile time error.
* **At least one component parameter.** Systems operate on components, so if none is provided it will result in a compile time error.
* **No duplicate components.** Having the same component more than once in the parameter list is likely an error on the programmers side, so a compile time error will be raised. 


## Parallel-by-default systems
Parallel systems can offer great speed-ups on multi-core machines, if the system in question has enough work to merit it. There is always some overhead associated with running code in multiple threads, and if the systems can not supply enough work for the threads you will end up losing performance instead. A good profiler can often help with this determination.

The dangers of multi-threaded code also exist in parallel systems, so take the same precautions here as you would in regular parallel code if your system accesses data outside the purview of rt.

Adding and removing components from entities in parallel systems is a thread-safe operation.

To disable parallelism in a system, use [`opts::not_parallel`](#optsnot_parallel).


## Automatic concurrency
Whenever a system is made, it will internally be scheduled for execution concurrently with other systems, if the systems dependencies permit it. Systems are always scheduled so they don't interfere with each other, and the order in which they are made is respected.

Dependencies are determined based on the components a system operate on.

If a component is written to, the system that previously read from or wrote to that component becomes a dependency, which means that the system must be run to completion before the new system can execute. This ensures that no data-races occur.

If a component is read from, the system that previously wrote to it becomes a dependency.

Multiple systems that read from the same component can safely run concurrently.


## The current entity[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/z9xYvd4Gc)
If you need access to the entity currently being processed by a system, make the first parameter type an `ecs::entity_id`. The entity will only be passed as a value, so trying to accept it as anything else will result in a compile time error.

```cpp
rt.make_system([](ecs::entity_id ent, greeting const& g) {
    std::cout << "entity with id " << ent << " says: " << g.msg << '\n';
});
```


## Sorting[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/Ge1PMaM97)
An additional function object can be passed along to `ecs::runtime::make_system` to specify the order in which components are processed. It must adhere to the [*Compare*](https://en.cppreference.com/w/cpp/named_req/Compare) requirements.

```cpp
ecs::runtime rt;

// sort descending
auto &sys_dec = rt.make_system(
    [](int const&) { /* ... */ },
    std::less<int>());

// sort ascending
auto & sys_asc = rt.make_system(
    [](int const&) { /* ... */ },
    std::greater<int>());

// sort length
auto &sys_pos = rt.make_system(
    [](position& pos, some_component const&) { /* ... */ },
    [](position const& p1, position const& p2) { return p1.length() < p2.length(); });
```

* Integers passed to `sys_dec` will arrive in descending order, from highest to lowest.
* Integers passed to `sys_asc` will arrive in ascending order.
* Positions passed to `sys_pos` will be sorted according to their length.<br>
  You could have sorted on the `some_component` instead.

Sorting functions must correspond to a type that is processed by the system, or an error will be raised during compilation.

**Note** Adding a sorting function takes up additional memory to maintain the sorted state, and it might adversely affect cache efficiency. Only use it if necessary.


## Filtering[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/zeqGa7nEv)
Entities can be filtered by marking the component you wish to filter as a pointer argument:
```cpp
rt.make_system([](int&, float*) { /* ... */ });
```
This system will run on all entities that has an `int` component and no `float` component.

More than one filter can be present; there is no limit.

**Note** `nullptr` is always passed to filtered components, so do not try and read from them.


## Hierarchies
Hierarchies can be created by adding the special component `ecs::parent` to an entity:
```cpp
rt.add_component({1}, ecs::parent{0});
```
This alone does not create a hierarchy, but it makes it possible for systems to act on this relationship data. To access the parent component in a system, add a `ecs::parent<>` parameter:

```cpp
rt.make_system([](entity_id id, parent<> const& p) {
  // id == 1, p.id() == 0
});
```
The angular brackets are needed because `ecs::parent` is a templated component which allows you to specify which, if any, of the parents components you would like access to.

### Accessing parent components[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/o9fer87x6)
A parents sub-components can be accessed by specifying them in a systems parent parameter. The components can the be accessed through the `get<T>` function on `ecs::parent`, where `T` specifies the type you want to access. If `T` is not specified in the sub-components of a systems parent parameter, an error will be raised.

If an `ecs::parent` has any non-filter sub-components the `ecs::parent` must always be taken as a value or a constant reference in systems, or an error will be reported.

 More than one sub-component can be specified; there is no upper limit.
```cpp
rt.add_component(0, short{1}, long{2});
rt.add_component(1, ecs::parent{0});

// Systems that only runs on entities that has a parent with a specific component,
rt.make_system([](ecs::parent<short, long> p) { // runs on entity 1
  /* 1 == p.get<short>() */
  /* 2 == p.get<long>() */
); 

// Fails: will not compile; no 'int' in 'p'
//rt.make_system([](ecs::parent<short> const& p) { p.get<int>(); });
```

### Filtering on parents components[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/o9fer87x6)
Filters work like regular component filters and can be specified on a parents sub-components:
```cpp
rt.make_system([](ecs::parent<short*> p) { });
```
An `ecs::parent` that only consist of filters does not need to be passed as a constant reference.


Marking the parent itself as a filter means that any entity with a parent component on it will be ignored. Any sub-components specified are ignored.
```cpp
rt.make_system([](int, ecs::parent<> *p) { });
```

### Traversal and layout
Hierarchies in this library are [topological sorted](https://en.wikipedia.org/wiki/Topological_sorting) and can be processed in parallel. An entity's parent is always processed before the entity itself.


# System options
The following options can be passed along to `make_system` calls in order to change the behaviour of a system. If an option is added more than once, only the first option is used.

### `opts::interval<ms, us>`[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/aGM86KWdf)
`opts::interval` is used to specify how often a system will run. The time duration between two runs may be higher than the specified times, but it will never be lower.

`opts::interval` takes a millisecond count and an optional microsecond count (defaults to zero). An `opts::interval<0, 0>` is considered to have no interval and will run every cycle.

```cpp
rt.make_system<ecs::opts::interval<16, 667>>([](int const&) {
    std::cout << "at least 16.667 ms has passed\n";
});
```


### `opts::group<group number>`[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/ezoq17fbr)
Systems can be segmented into groups by passing along `opts::group<N>`, where `N` is a compile-time integer constant, as a template parameter to `ecs::make_system`. Systems are roughly executed in the order they are made, but groups ensure absolute separation of systems. Systems with no group id specified are put in group 0.

```cpp
rt.make_system<ecs::opts::group<1>>([](int const&) {
    std::cout << "hello from group one\n";
});
rt.make_system<ecs::opts::group<-1>>([](int const&) {
    std::cout << "hello from group negative one\n";
});
rt.make_system([](int const&) { // same as 'ecs::opts::group<0>'
    std::cout << "hello from group whatever\n";
});
// ...
rt.add_component(0, int{});
rt.update();
```

Running the above code will print out
> hello from group negative one\
> hello from group whatever\
> hello from group one

**Note:** systems from different groups are never executed concurrently, and all systems in one group will run to completion before the next group is run.


### `opts::manual_update`[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/TxvndcTEq)
Systems marked as being manually updated will not be added to the scheduler, and will thus require the user to call the `system::run()` function themselves.
Calls to `ecs::runtime::commit_changes()` will still cause the system to respond to changes in components.

```cpp
auto& manual_sys = rt.make_system<ecs::opts::manual_update>([](int const&) { /* ... */ });
// ...
rt.add_component(0, int{});
rt.update(); // will not run 'manual_sys'
manual_sys.run(); // required to run the system
```

### `opts::not_parallel`[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/MK9xcTedq)
This option will prevent a system from processing components in parallel, which can be beneficial when a system does little work.

It should not be used to avoid data races when writing to a shared variable not under ecs control, such as a global variable or variables catured be reference in system lambdas. Use atomics, mutexes, or even [`tls::collect`](https://github.com/kgorking/tls/blob/master/examples/collect/accumulate/accumulate.cpp) in these cases, if possible.

# Component Flags
The behavior of components can be changed by using component flags, which can change how they are managed internally and can offer performance and memory benefits.

### `tag`[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/dj8WjTWbE)
Marking a component as a *tag* is used for components that signal some kind of state, without needing to
take up any memory. For instance, you could use it to tag certain entities as having some form of capability,
like a 'freezable' tag to mark stuff that can be frozen.

```cpp
struct freezable {
    using ecs_flags = ecs::flags<ecs::tag>;
};
```

Using tagged components in systems has a slightly different syntax to regular components, namely that they are
always passed by value. This is to discourage the use of tags to share some kind of data, which you should use the `global` flag
for instead.

```cpp
rt.make_system([](greeting const& g, freezable) {
  // code to operate on entities that has a greeting and are freezable
});
```

If tag components are marked as anything other than pass-by-value, the compiler will drop a little error message to remind you.

### `immutable`[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/rnbsooorb)
Marking a component as *immutable* (a.k.a. const) is used for components that are not to be changed by systems.
This is used for passing read-only data to systems. If a component is marked as `immutable` and is used in a system without being marked `const`, you will get a compile-time error reminding you to make it constant.

### `transient`[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/W7hvrnjT6)
Marking a component as *transient* is used for components that only exists on entities temporarily. The runtime will remove these components
from entities automatically after one cycle.
```cpp
struct damage {
    using ecs_flags = ecs::flags<ecs::transient>;
    double value;
};
// ...
rt.add_component({0,99}, damage{9001});
rt.commit_changes(); // adds the 100 damage components
rt.commit_changes(); // removes the 100 damage components
```

### `global`[<img src="https://godbolt.org/favicon.ico" width="32">](https://godbolt.org/z/4Y9zsMPe1)
Marking a component as *global* is used for components that hold data that is shared between all systems the component is added to, without the need to explicitly add the component to any entity. Adding global components to entities is not possible.

```cpp
struct frame_data {
    using ecs_flags = ecs::flags<ecs::global>;
    double delta_time = 0.0;
};
// ...
rt.add_component({0, 100}, position{}, velocity{});
// ...
rt.make_system([](position& pos, velocity const& vel, frame_data const& fd) {
    pos += vel * fd.delta_time;
});
```

Global components can contain `std::atomic<>` types, unlike regular components, due to the fact that they are never copied.

#### Global systems
With [global components](#global) it is also possible to create *global systems*, which are a special kind of system that only operates on global components. Global systems are only run once per update cycle.

```cpp
rt.make_system([](frame_data& fd) {
    static auto clock_last = std::chrono::high_resolution_clock::now();
    auto const clock_now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const diff = clock_now - clock_last;
    fd.delta_time = clock_diff.count();
});
```

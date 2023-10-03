#include <ecs/ecs.h>
#include <iostream>
#include <ecs/variant.h>
#include <typeinfo>

struct A { };
struct B { using variant_of = A; };
struct C { using variant_of = B; };

static_assert(!ecs::detail::is_variant<A>);
static_assert(ecs::detail::is_variant<B>);
static_assert(ecs::detail::is_variant<C>);
static_assert(ecs::detail::is_variant_of<A, B>());
static_assert(ecs::detail::is_variant_of<A, C>());
static_assert(ecs::detail::is_variant_of<B, A>());
static_assert(ecs::detail::is_variant_of<B, C>());
static_assert(ecs::detail::is_variant_of<C, A>());
static_assert(ecs::detail::is_variant_of<C, B>());

int main() {
	ecs::runtime rt;

	rt.make_system([](A) { std::cout << 'A'; });
	rt.make_system([](B) { std::cout << 'B'; });
	rt.make_system([](C) { std::cout << 'C'; });

	// Print A
	rt.add_component(0, A{});
	rt.update();

	// Print 'C'
	rt.add_component(0, C{});
	rt.update();

	// Print 'B'
	rt.add_component(0, B{});
	rt.update();

	// Print nothing
	rt.remove_component<C>(0);
	rt.update();
}

#include <ecs/runtime.h>

struct test_tag {
	using ecs_flags = ecs::flags<ecs::tag>;
};
static_assert(ecs::detail::tagged<test_tag>);

struct test_transient {
	using ecs_flags = ecs::flags<ecs::transient>;
};
static_assert(ecs::detail::transient<test_transient>);

struct test_immutable {
	using ecs_flags = ecs::flags<ecs::immutable>;
};
static_assert(ecs::detail::immutable<test_immutable>);
static_assert(ecs::detail::immutable<test_immutable&>);
static_assert(ecs::detail::immutable<test_immutable const&>);
static_assert(ecs::detail::immutable<test_immutable*>);

struct test_global {
	using ecs_flags = ecs::flags<ecs::global>;
};
static_assert(ecs::detail::global<test_global>);

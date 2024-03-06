#ifndef ECS_FLAGS_H
#define ECS_FLAGS_H

namespace ecs {
ECS_EXPORT enum ComponentFlags {
	// Add this in a component to mark it as tag.
	// Uses O(1) memory instead of O(n).
	// Mutually exclusive with 'share' and 'global'
	tag = 1 << 0,

	// Add this in a component to mark it as transient.
	// The component will only exist on an entity for one cycle,
	// and then be automatically removed.
	// Mutually exclusive with 'global'
	transient = 1 << 1,

	// Add this in a component to mark it as constant.
	// A compile-time error will be raised if a system tries to
	// write to the component through a reference.
	immutable = 1 << 2,

	// Add this in a component to mark it as global.
	// Global components can be referenced from systems without
	// having been added to any entities.
	// Uses O(1) memory instead of O(n).
	// Mutually exclusive with 'tag', 'share', and 'transient'
	global = 1 << 3
};

ECS_EXPORT template <ComponentFlags... Flags>
struct flags {
	static constexpr int val = (Flags | ...);
};
}

// Some helper concepts/struct to detect flags
namespace ecs::detail {

// Strip a type of all its modifiers
template <typename T>
using stripped_t = std::remove_pointer_t<std::remove_cvref_t<T>>;


template <typename T>
concept tagged = ComponentFlags::tag == (stripped_t<T>::ecs_flags::val & ComponentFlags::tag);

template <typename T>
concept transient = ComponentFlags::transient == (stripped_t<T>::ecs_flags::val & ComponentFlags::transient);

template <typename T>
concept immutable = ComponentFlags::immutable == (stripped_t<T>::ecs_flags::val & ComponentFlags::immutable);

template <typename T>
concept global = ComponentFlags::global == (stripped_t<T>::ecs_flags::val & ComponentFlags::global);

template <typename T>
concept local = !global<T>;

template <typename T>
concept persistent = !transient<T>;

template <typename T>
concept unbound = (tagged<T> || global<T>); // component is not bound to a specific entity (ie static)

//
template <typename T>
struct is_tagged : std::bool_constant<tagged<T>> {};

template <typename T>
struct is_transient : std::bool_constant<transient<T>> {};

template <typename T>
struct is_immutable : std::bool_constant<immutable<T>> {};

template <typename T>
struct is_global : std::bool_constant<global<T>> {};

template <typename T>
struct is_local : std::bool_constant<!global<T>> {};

template <typename T>
struct is_persistent : std::bool_constant<!transient<T>> {};

template <typename T>
struct is_unbound : std::bool_constant<tagged<T> || global<T>> {};

// Returns true if a type is read-only
template <typename T>
constexpr bool is_read_only_v = detail::immutable<T> || detail::tagged<T> || std::is_const_v<std::remove_reference_t<T>>;
} // namespace ecs::detail


// Unittest
namespace {
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
} // namespace
#endif // !ECS_FLAGS_H

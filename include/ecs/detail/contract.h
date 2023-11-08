#ifndef ECS_CONTRACT_H
#define ECS_CONTRACT_H

#include <concepts>
#include <iostream>
#if __has_include(<stacktrace>)
#include <stacktrace>
#endif

// Contracts. If they are violated, the program is in an invalid state and is terminated.
namespace ecs::detail {
// Concept for the contract violation interface.
template <typename T>
concept contract_violation_interface = requires(T t) {
	{ t.assertion_failed("", "") } -> std::same_as<void>;
	{ t.precondition_violation("", "") } -> std::same_as<void>;
	{ t.postcondition_violation("", "") } -> std::same_as<void>;
};

struct default_contract_violation_impl {
	void panic(char const* why, char const* what, char const* how) noexcept {
		std::cerr << why << ": \"" << how << "\"\n\t" << what << "\n\n";
#ifdef __cpp_lib_stacktrace
		// Dump a stack trace if available
		std::cerr << "** stack dump **\n" << std::stacktrace::current(3) << '\n';
#endif
		std::terminate();
	}

	void assertion_failed(char const* what, char const* how) noexcept {
		panic("Assertion failed", what, how);
	}

	void precondition_violation(char const* what, char const* how) noexcept {
		panic("Precondition violation", what, how);
	}

	void postcondition_violation(char const* what, char const* how) noexcept {
		panic("Postcondition violation", what, how);
	}
};
} // namespace ecs::detail

// The contract violation interface, which can be overridden by users
ECS_EXPORT namespace ecs {
template <typename...>
auto contract_violation_handler = ecs::detail::default_contract_violation_impl{};
}

#if defined(ECS_ENABLE_CONTRACTS)

namespace ecs::detail {
template <typename... DummyArgs>
	requires(sizeof...(DummyArgs) == 0)
inline void do_assertion_failed(char const* what, char const* how) {
	ecs::detail::contract_violation_interface auto& cvi = ecs::contract_violation_handler<DummyArgs...>;
	cvi.assertion_failed(what, how);
}

template <typename... DummyArgs>
	requires(sizeof...(DummyArgs) == 0)
inline void do_precondition_violation(char const* what, char const* how) {
	ecs::detail::contract_violation_interface auto& cvi = ecs::contract_violation_handler<DummyArgs...>;
	cvi.precondition_violation(what, how);
}

template <typename... DummyArgs>
	requires(sizeof...(DummyArgs) == 0)
inline void do_postcondition_violation(char const* what, char const* how) {
	ecs::detail::contract_violation_interface auto& cvi = ecs::contract_violation_handler<DummyArgs...>;
	cvi.postcondition_violation(what, how);
}
} // namespace ecs::detail


#define Assert(expression, message)                                                                                                        \
	do {                                                                                                                                   \
		((expression) ? static_cast<void>(0) : ecs::detail::do_assertion_failed(#expression, message));                                    \
	} while (false)

#define Pre(expression, message)                                                                                                           \
	do {                                                                                                                                   \
		((expression) ? static_cast<void>(0) : ecs::detail::do_precondition_violation(#expression, message));                              \
	} while (false)

#define Post(expression, message)                                                                                                          \
	do {                                                                                                                                   \
		((expression) ? static_cast<void>(0) : ecs::detail::do_postcondition_violation(#expression, message));                             \
	} while (false)

// Audit contracts. Used for expensive checks; can be disabled.
#if defined(ECS_ENABLE_CONTRACTS_AUDIT)
#define AssertAudit(expression, message) Assert(expression, message)
#define PreAudit(expression, message) Pre(expression, message)
#define PostAudit(expression, message) Post(expression, message)
#else
#define AssertAudit(expression, message)
#define PreAudit(expression, message)
#define PostAudit(expression, message)
#endif

#else

#if __has_cpp_attribute(assume)
#define ASSUME(expression) [[assume((expression))]]
#else
#ifdef _MSC_VER
#define ASSUME(expression) __assume((expression))
#elif defined(__clang)
#define ASSUME(expression) __builtin_assume((expression))
#elif defined(GCC)
#define ASSUME(expression) __attribute__((assume((expression))))
#else
#define ASSUME(expression) /* unknown */
#endif
#endif // __has_cpp_attribute

#define Assert(expression, message) ASSUME(expression)
#define Pre(expression, message) ASSUME(expression)
#define Post(expression, message) ASSUME(expression)
#define AssertAudit(expression, message) ASSUME(expression)
#define PreAudit(expression, message) ASSUME(expression)
#define PostAudit(expression, message) ASSUME(expression)
#endif

#endif // !ECS_CONTRACT_H

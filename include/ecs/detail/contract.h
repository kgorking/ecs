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
	{ t.assertion_failed("") } -> std::same_as<void>;
	{ t.precondition_violation("") } -> std::same_as<void>;
	{ t.postcondition_violation("") } -> std::same_as<void>;
};

struct default_contract_violation_impl {
	void panic(char const* why, char const* what) {
		if (std::is_constant_evaluated()) {
			// During compile-time just "throw" an exception, which will halt compilation
			// static_assert(!why, what);
			throw;
		} else {
			std::cerr << why << " - '" << what << "'\n";
#ifdef __cpp_lib_stacktrace
			// Dump a stack trace if available
			std::cerr << std::stacktrace::current(2) << '\n';
#endif
		}
	}

	void assertion_failed(char const* what) {
		panic("Assertion failed", what);
	}

	void precondition_violation(char const* what) {
		panic("Precondition violation", what);
	}

	void postcondition_violation(char const* what) {
		panic("Postcondition violation", what);
	}
};
} // namespace ecs::detail

// The contract violation interface, which can be overridden by users

template <typename...>
inline auto contract_violation_handler = ecs::detail::default_contract_violation_impl{};

namespace ecs::detail {
template <typename... DummyArgs>
	requires(sizeof...(DummyArgs) == 0)
inline void do_assertion_failed(char const* what) {
	ecs::detail::contract_violation_interface auto& cvi = contract_violation_handler<DummyArgs...>;
	cvi.assertion_failed(what);
	std::terminate();
}

template <typename... DummyArgs>
	requires(sizeof...(DummyArgs) == 0)
inline void do_precondition_violation(char const* what) {
	ecs::detail::contract_violation_interface auto& cvi = contract_violation_handler<DummyArgs...>;
	cvi.precondition_violation(what);
	std::terminate();
}

template <typename... DummyArgs>
	requires(sizeof...(DummyArgs) == 0)
inline void do_postcondition_violation(char const* what) {
	ecs::detail::contract_violation_interface auto& cvi = contract_violation_handler<DummyArgs...>;
	cvi.postcondition_violation(what);
	std::terminate();
}
} // namespace ecs::detail

#if defined(ECS_ENABLE_CONTRACTS)

#define Assert(cond)                                                                                                                       \
	do {                                                                                                                                   \
		((cond) ? static_cast<void>(0) : ecs::detail::do_assertion_failed(#cond));                                                         \
	} while (false)

#define Pre(cond)                                                                                                                          \
	do {                                                                                                                                   \
		((cond) ? static_cast<void>(0) : ecs::detail::do_precondition_violation(#cond));                                                   \
	} while (false)

#define Post(cond)                                                                                                                         \
	do {                                                                                                                                   \
		((cond) ? static_cast<void>(0) : ecs::detail::do_postcondition_violation(#cond));                                                  \
	} while (false)

// Audit contracts. Used for expensive checks; can be disabled.
#if defined(ECS_ENABLE_CONTRACTS_AUDIT)
#define AssertAudit(cond) Assert(cond)
#define PreAudit(cond) Pre(cond)
#define PostAudit(cond) Post(cond)
#else
#define AssertAudit(cond)
#define PreAudit(cond)
#define PostAudit(cond)
#endif

#else

#if __has_cpp_attribute(assume)
#define ASSUME(cond) [[assume(cond)]]
#else
#ifdef _MSC_VER
#define ASSUME(cond) __assume((cond))
#elif defined(__clang)
#define ASSUME(cond) __builtin_assume((cond))
#elif defined(GCC)
#define ASSUME(cond) __attribute__((assume((cond))))
#else
#define ASSUME(cond) /* unknown */
#endif
#endif // __has_cpp_attribute

#define Assert(cond) ASSUME(cond)
#define Pre(cond) ASSUME(cond)
#define Post(cond) ASSUME(cond)
#define AssertAudit(cond) ASSUME(cond)
#define PreAudit(cond) ASSUME(cond)
#define PostAudit(cond) ASSUME(cond)
#endif

#endif // !ECS_CONTRACT_H

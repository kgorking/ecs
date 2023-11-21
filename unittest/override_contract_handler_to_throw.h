#pragma once

// Override the default handler for contract violations.
struct unittest_handler {
	void assertion_failed(char const* , char const* msg)        { throw std::runtime_error(msg); }
	void precondition_violation(char const* , char const* msg)  { throw std::runtime_error(msg); }
	void postcondition_violation(char const* , char const* msg) { throw std::runtime_error(msg); }
};

#if !(defined(ECS_USE_MODULES) && defined(__clang__)) // currently bugged in clang
template <>
auto ecs::contract_violation_handler<> = unittest_handler{};
#endif

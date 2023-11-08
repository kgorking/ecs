#include <ecs/ecs.h>
#include <iostream>

// An example handler for contract violations. Dumps input to std::cout.
// All 3 functions shown below must be present in custom handlers.
struct example_handler {
	void assertion_failed(char const* sz, char const* msg) {
		std::cout << "assert (" << sz << "): " << msg << '\n';
		std::terminate();
	}

	void precondition_violation(char const* sz, char const* msg) {
		std::cout << "precondition (" << sz << "): " << msg << '\n';
		std::terminate();
	}

	void postcondition_violation(char const* sz, char const* msg) {
		std::cout << "postcondition (" << sz << "): " << msg << '\n';
		std::terminate();
	}
};

// Override the default handler for contract violations.
// Comment this out to use the default handler.
#ifndef __clang__ // currently bugged in clang
template <> auto ecs::contract_violation_handler<> = example_handler{};
#endif

int main() {
	// trigger a pre-condition
	ecs::runtime rt;
	rt.add_component(0, 0);
	rt.add_component(0, 0);
	rt.commit_changes();
}

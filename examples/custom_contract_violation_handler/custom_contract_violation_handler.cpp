#include <ecs/detail/contract.h>
#include <iostream>

// An example handler for contract violations. Dumps input to std::cout.
// std::terminate() is always called right after these functions complete.
// All 3 functions shown below must be present in custom handlers.
struct example_handler {
	void assertion_failed(char const* sz, char const* msg) {
		std::cout << "assert (" << sz << "): " << msg << '\n';
	}

	void precondition_violation(char const* sz, char const* msg) {
		std::cout << "precondition (" << sz << "): " << msg << '\n';
	}

	void postcondition_violation(char const* sz, char const* msg) {
		std::cout << "postcondition (" << sz << "): " << msg << '\n';
	}
};

// Override the default handler for contract violations.
// Comment this out to use the default handler.
template <> auto contract_violation_handler<> = example_handler{};


int main() {
	// trigger assert
	Assert(false, "test assert");

	// trigger pre-condition
	Pre(false, "test precondition");

	// trigger post-condition
	Post(false, "test postcondition");
}

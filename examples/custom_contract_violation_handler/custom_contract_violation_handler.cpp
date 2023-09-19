#include <ecs/detail/contract.h>
#include <iostream>


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

// Override the default contract violation handler
template <>
inline auto contract_violation_handler<> = example_handler{};


int main() {
	// trigger assert
	Assert(false, "test assert");

	// trigger pre-condition
	Pre(false, "test precondition");

	// trigger post-condition
	Post(false, "test postcondition");
}

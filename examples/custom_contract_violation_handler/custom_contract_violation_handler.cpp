#include <ecs/ecs.h>
#include <iostream>

struct contract_violation_impl {
	void assertion_failed(char const* sz) {
		puts("Fancy assertion failed");
		puts(sz);
	}

	void precondition_violation(char const* sz) {
		puts("Fancy precondition violation");
		puts(sz);
	}

	void postcondition_violation(char const* sz) {
		puts("Fancy postcondition violation");
		puts(sz);
	}
};

template <>
inline auto contract_violation_handler<> = contract_violation_impl{};

int main() {
	ecs::runtime rt;
	rt.remove_component<int>({0});
}

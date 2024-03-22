#include <array>
#include <catch2/catch_test_macros.hpp>
#include <ecs/detail/listarray.h>
#include <iostream>
#include <queue>
#include <ranges>
#include <stack>

using ecs::detail::listarray;

struct node {
	int test;
	int data;
	node* next;
};

struct stepper {
	int target;
	int destination;
	int size;
	bool operator<(stepper other) const {
		return (target > other.target);
	}
};

TEST_CASE("Gorking list") {
	constexpr int N = 100;
	std::array<node, N> nodes;
	std::priority_queue<stepper> stack;

	int const log_n = (int)std::ceil(std::log2(N));
	std::cout << "log " << N << " =" << log_n << '\n';

	// Load up log(N)-1 steppers
	for (int i = 0; i < log_n; i++) {
		int const step = ((N - 1) / (int)std::pow(2, i));
		stack.emplace(i + step, i, step);
	}

	for (int i = 0; i < N; i += 1) {
		nodes[i] = {-1, i, nullptr};

		while (!stack.empty() && stack.top().target == i) {
			auto st = stack.top();
			stack.pop();
			if (i > nodes[st.destination].test)
				nodes[st.destination].test = i;
			st.target += st.size;
			st.destination = i;
			if (st.target < N)
				stack.push(st);
		}
	}

	bool const broken = !stack.empty();
	while (!stack.empty()) {
		std::cout << "garbage: " << stack.top().target << ", " << stack.top().destination << '\n';
		stack.pop();
	}

	for (node& n : nodes) {
		std::cout << n.data << " -> " << n.test << "  distance: " << (n.test-n.data) << '\n';
		//		std::cout << n.test << '\t';
	}

	CHECK(!broken);
}

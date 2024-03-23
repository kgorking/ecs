#include <array>
#include <catch2/catch_test_macros.hpp>
#include <ecs/detail/listarray.h>
#include <iostream>
#include <queue>
#include <ranges>
#include <stack>

using ecs::detail::listarray;

struct node {
	node* next_power;
	node* next;
	int data;
};

struct stepper {
	int target;
	int size;
	int offset;
	node* from;
	bool operator<(stepper other) const {
		return (other.target < target);
	}
};

int search(node* n, int val) {
	std::cout << n->data;

	int steps = 0;
	while (val > n->data) {
		if (val >= n->next_power->data) {
			n = n->next_power;
			std::cout << " -> " << n->data;
		} else {
			n = n->next;
			std::cout << " -> " << n->data;
		}
		steps += 1;
	}
	return (val == n->data) ? steps : -1;
}

TEST_CASE("Gorking list") {
	constexpr int N = 127;
	std::array<node, N> nodes;

	int log_n = (int)std::ceil(std::log2(N));
	// int log_n = 1;
	// while (N >= (1 << log_n))
	//	log_n += 1;
	std::cout << "log " << N << " =" << log_n << '\n';
	std::cout << "1<<log_n " << (1 << log_n) << '\n';

	// Load up steppers
	std::priority_queue<stepper> stack;
	stack.emplace(N - 1, N - 1, 0, &nodes[0]);
	for (int i = 1; i < log_n + 1; i++) {
		int const step = 1 << (log_n - i);
		stack.emplace(i + step, step, i, &nodes[i]);
	}

	for (int i = 0; i < N; i += 1) {
		node* next = (i < N - 1) ? &nodes[i + 1] : nullptr;
		nodes[i] = {nullptr, next, i};

		while (!stack.empty() && stack.top().target == i) {
			auto st = stack.top();
			stack.pop();

			st.from->next_power = &nodes[i];
			st.target = i + st.size;

			if (st.target < N) {
				st.from = &nodes[i];
				stack.push(st);
			}
		}
	}

	CHECK(stack.empty());

#if 0
	for (node& n : nodes) {
		std::cout << n.data;
		if (n.next_power != nullptr) {
			auto const dist = std::distance(&nodes[n.data], n.next_power);
			std::cout << " -> " << (n.data + dist);
			std::cout << "  distance: " << dist;
		}
		std::cout << '\n';
	}
#endif
	node* root = &nodes[0];
#if 1
	int total_steps = 0;
	int max_steps = 0;
	for (node& n : nodes) {
		std::cout << "search '" << n.data << "' : \t";
		int steps = search(root, n.data);
		total_steps += steps;
		max_steps = std::max(max_steps, steps);
		std::cout << " (" << steps << ")\n";
	}
	std::cout << "\nMaximum steps: " << max_steps << '\n';
	std::cout << "Total steps: " << total_steps << '\n';
#else
	static_assert(N == 100);
	std::cout << '\n';
	std::cout << "steps: " << search(root, 37) << '\n';
	std::cout << "steps: " << search(root, 83) << '\n';
#endif
}

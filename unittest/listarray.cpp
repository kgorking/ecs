#include <array>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <ecs/detail/listarray.h>
#include <iostream>
#include <queue>
#include <ranges>
#include "ecs/detail/gorking_list.h"

using ecs::detail::listarray;

constexpr int log_2(std::unsigned_integral auto i) {
	return sizeof(i) * 8 - std::countl_zero(i); // (int)std::ceil(std::log2(N));
}

struct node {
	node* next_power;
	node* next;
	int data;
};

struct stepper {
	int target;
	int size;
	node* from;
	bool operator<(stepper other) const {
		return (other.target < target);
	}
};

int search(node* n, int val) {
	std::cout << n->data;

	if (val < n->data || val > n->next_power->data)
		return -1;

	int steps = 0;
	while (val > n->data) {
		if (val >= n->next_power->data) {
			n = n->next_power;
		} else {
			n = n->next;
		}
		std::cout << " -> " << n->data;
		steps += 1;
	}
	return (val == n->data) ? steps : -1;
}

TEST_CASE("Gorking list") {
	constexpr unsigned int N = 78;
	int constexpr log_n = std::bit_width(N);

	std::array<node, N> nodes{};
	std::priority_queue<stepper> stack;

	// Init linked list
	for (int i = 0; i < N - 1; i += 1) {
		node* const next = &nodes[i + 1];
		nodes[i] = {next, next, i};
	}
	nodes[N - 1] = {&nodes[N - 1], nullptr, N - 1};

	// Load up steppers
	stack.emplace(N - 1, N - 1, &nodes[0]);
	for (int i = 1; i < log_n && i < N - 1; i++) {
		int const step = 1 << (log_n - i);
		stack.emplace(i + step, step, &nodes[i]);
	}

	// Set up the jump points
	node* current = &nodes[0];
	int i = 0;
	while (current->next != nullptr) {
		while (stack.top().target == i) {
			stepper st = stack.top();
			stack.pop();

			st.from->next_power = current->next;

			st.from = current;
			st.target = i + st.size;
			stack.push(st);
		}

		i += 1;
		current = current->next;
	}
	while (!stack.empty()) {
		stack.top().from->next_power = current;
		stack.pop();
	}

	CHECK(stack.empty());

#if 1
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
#if 1
	node* root = &nodes[0];
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
	std::cout << "Total steps  : " << total_steps << '\n';
#endif
#if 1
	ecs::detail::gorking_list<int> list(std::views::iota(-2, 100));
	for (int const val : std::views::iota(-2, 100))
		REQUIRE(list.contains(val));
	REQUIRE(!list.contains(-3));
	REQUIRE(!list.contains(101));
#endif
}

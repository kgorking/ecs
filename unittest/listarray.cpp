#include "ecs/detail/gorking_list.h"
#include <array>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <ecs/detail/listarray.h>
#include <iostream>
#include <queue>
#include <ranges>

using ecs::detail::listarray;

struct node {
	node* next[2];
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

	if (val < n->data || val > n->next[1]->data)
		return -1;

	int steps = 0;
	while (val > n->data) {
		n = n->next[val >= n->next[1]->data];
		std::cout << " -> " << n->data;
		steps += 1;
	}
	return (val == n->data) ? steps : -1;
}

TEST_CASE("Gorking list") {
	constexpr int N = 102;
	int constexpr log_n = std::bit_width((unsigned)N);

	std::array<node, N> nodes{};

	// Init linked list
	for (int i = 0; i < N - 1; i += 1) {
		node* const next = &nodes[i + 1];
		nodes[i] = {{next, next}, i};
	}
	nodes[N - 1] = {{nullptr, &nodes[N - 1]}, N - 1};

	// Load up steppers
	node* current = &nodes[0];
	stepper stack[32];
	for (int i = 0; i < log_n; i++) {
		int const step = 1 << (log_n - i);
		stack[log_n - 1 - i] = {i + step, step, current};
		current = current->next[0];
	}
	// std::make_heap(stack, stack + log_n);

	// Set up the jump points
	int i = 0;
	current = &nodes[0];
	stepper* min_step = &stack[log_n - 1];
	while (current->next[0] != nullptr) {
		while (stack[0].target == i) {
			std::pop_heap(stack, stack + log_n);
			min_step->from->next[1] = current->next[0];
			min_step->from = current;
			min_step->target = i + min_step->size;
			std::push_heap(stack, stack + log_n);
		}

		i += 1;
		current = current->next[0];
	}
	for (i = 0; i < log_n; i++) {
		stack[i].from->next[1] = current;
	}

#if 1
	for (node& n : nodes) {
		std::cout << n.data;
		if (n.next[1] != nullptr) {
			auto const dist = std::distance(&nodes[n.data], n.next[1]);
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

#if 0
	ecs::detail::gorking_list<int> list(std::views::iota(-2, 100));
	for (int const val : std::views::iota(-2, 100))
		REQUIRE(list.contains(val));
	REQUIRE(!list.contains(-3));
	REQUIRE(!list.contains(101));

	list.insert(101);
	REQUIRE(list.contains(101));
	list.insert(-3);
	REQUIRE(list.contains(-3));
	list.insert(22);
	REQUIRE(list.contains(22));

	ecs::detail::gorking_list<int> list2;
	for (int const val : std::views::iota(-2, 100))
		list2.insert(val);
	for (int val : list2)
		val++;
	REQUIRE(list2.contains(83));

	list2.remove(83);
	REQUIRE(list2.contains(82));
	REQUIRE(!list2.contains(83));
	REQUIRE(list2.contains(84));

#endif
}

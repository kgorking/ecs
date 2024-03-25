#ifndef ECS_DETAIL_GORKING_LIST_H
#define ECS_DETAIL_GORKING_LIST_H

#include <queue>
#include <ranges>
#include <memory>
#include <cassert>
#include <algorithm>

namespace ecs::detail {
	template <typename T>
	struct gorking_list {
		constexpr gorking_list(std::ranges::sized_range auto const& range) {
			assert(std::ranges::is_sorted(range));

			size = std::ssize(range);

			node* curr = nullptr;
			for (auto val : range) {
				node* n = new node{{nullptr, nullptr}, val};
				if (!curr) {
					root = curr = n;
				} else {
					curr->next[0] = n;
					curr->next[1] = n;
					curr = n;
				}
			}

			rebalance();
		}

		constexpr ~gorking_list() {
			node* n = root;
			while (n) {
				node* next = n->next[0];
				delete n;
				n = next;
			}
		}

		constexpr void rebalance() {
			if (!root)
				return;

			int const log_n = std::bit_width<std::uintptr_t>(size);

			struct stepper {
				std::intptr_t target;
				std::intptr_t size;
				node* from;
				constexpr bool operator<(stepper const& a) const {
					return (a.target < target);
				}
			};

			// Load up steppers
			stepper steppers[32];
			node* current = root;
			for (int i = 0; i < log_n; i++) {
				int const step = 1 << (log_n - i);
				steppers[log_n-1-i] = {i + step, step, current};
				current = current->next[0];
			}
			//std::ranges::make_heap(steppers, steppers + log_n, std::less{});

			// Set up the jump points
			current = root;
			std::intptr_t i = 0;
			stepper* min_step = &steppers[log_n - 1];
			while (current->next[0] != nullptr) {
				while (steppers[0].target == i) {
					std::pop_heap(steppers, steppers + log_n);
					min_step->from->next[1] = current->next[0];
					min_step->from = current;
					min_step->target += min_step->size;
					std::push_heap(steppers, steppers + log_n);
				}

				i += 1;
				current = current->next[0];
			}
			for (i = 0; i < log_n; i++) {
				steppers[i].from->next[1] = current;
			}
		}

		constexpr bool contains(T const& val) const {
			node* n = root;
			while (val > n->data)
				n = n->next[val >= n->next[1]->data];
			return (val == n->data);
		}

	private:
		constexpr struct node* find(T val) const {
			node* n = root;
			while (val > n->data)
				n = n->next[val >= n->next[1]->data];
			return (val == n->data) ? n : nullptr;
		}

		struct node {
			node* next[2]{};
			T data{};
		};

		node* root = nullptr;
		std::intptr_t size = 0;
	};

	static_assert(gorking_list<int>(std::views::iota(-2, 2)).contains(-1));
} // namespace ecs::detail

#endif // !ECS_DETAIL_GORKING_LIST_H

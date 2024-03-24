#ifndef ECS_DETAIL_GORKING_LIST_H
#define ECS_DETAIL_GORKING_LIST_H

#include <queue>
#include <ranges>
#include <memory>
#include <cassert>

namespace ecs::detail {
	template <typename T>
	struct gorking_list {
		constexpr gorking_list(std::ranges::sized_range auto const& range) {
			assert(std::ranges::is_sorted(range));

			size = std::ssize(range);

			node* curr = nullptr;
			for (auto val : range) {
				auto n = std::make_unique<node>(nullptr, nullptr, val);
				if (!curr) {
					root = std::move(n);
					curr = root.get();
				} else {
					curr->next = std::move(n);
					curr->next_power = curr->next.get();
					curr = curr->next.get();
				}
			}

			rebalance();
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
			node* current = root.get();
			for (int i = 0; i < log_n; i++) {
				int const step = 1 << (log_n - i);
				steppers[log_n-1-i] = {i + step, step, current};
				current = current->next.get();
			}
			//std::ranges::make_heap(steppers, steppers + log_n, std::less{});

			// Set up the jump points
			current = root.get();
			std::intptr_t i = 0;
			stepper* min_step = &steppers[log_n - 1];
			while (current->next != nullptr) {
				while (steppers[0].target == i) {
					std::pop_heap(steppers, steppers + log_n);
					min_step->from->next_power = current->next.get();
					min_step->from = current;
					min_step->target += min_step->size;
					std::push_heap(steppers, steppers + log_n);
				}

				i += 1;
				current = current->next.get();
			}
			for (i = 0; i < log_n; i++) {
				steppers[i].from->next_power = current;
			}
		}

		constexpr bool contains(T const& val) const {
			if (val < root->data || val > root->next_power->data)
				return false;

			node* n = root.get();
			while (val > n->data) {
				if (val >= n->next_power->data) {
					n = n->next_power;
				} else {
					n = n->next.get();
				}
			}

			return (val == n->data);
		}

	private:
		constexpr struct node* find(T val) const {
			if (val < root->data || val > root->next_power->data)
				return nullptr;

			node* n = root.get();
			while (val > n->data) {
				if (val >= n->next_power->data) {
					n = n->next_power;
				} else {
					n = n->next.get();
				}
			}

			return (val == n->data) ? n : nullptr;
		}

		struct node {
			//node* next[2];
			std::unique_ptr<node> next;
			node* next_power;
			T data;
		};

		std::unique_ptr<node> root = nullptr;
		std::intptr_t size = 0;
	};

	static_assert(gorking_list<int>(std::views::iota(-2, 2)).contains(-1));
} // namespace ecs::detail

#endif // !ECS_DETAIL_GORKING_LIST_H

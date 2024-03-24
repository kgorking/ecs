#ifndef ECS_DETAIL_GORKING_LIST_H
#define ECS_DETAIL_GORKING_LIST_H

#include <queue>
#include <ranges>

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
				bool operator<(stepper const& a) const {
					return (a.target < target);
				}
			};

			// Load up steppers
			//std::vector<stepper> stack(log_n);
			stepper stack[32];
			stack[0] = {size - 1, size - 1, root.get()};
			node* current = root->next.get();
			for (int i = 1; i < log_n && i < size - 1; i++) {
				int const step = 1 << (log_n - i);
				stack[i] = {i + step, step, current};
				current = current->next.get();
			}
			//std::ranges::make_heap(stack, stack + log_n, std::less{});

			// Set up the jump points
			current = root.get();
			std::intptr_t i = 0;
			while (current->next != nullptr) {
				int index = log_n - 1;
				while (stack[index].target == i) {
					stepper& st = stack[index];
					index -= 1;

					//std::ranges::pop_heap(stack, stack + index, std::less{});

					st.from->next_power = current->next.get();

					st.from = current;
					st.target = i + st.size;
					//std::ranges::push_heap(stack, stack + index + 1, std::less{});
				}

				i += 1;
				current = current->next.get();
			}
			for (i = 0; i < log_n; i++) {
				stack[i].from->next_power = current;
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
		struct node {
			std::unique_ptr<node> next;
			node* next_power;
			T data;
		};

		std::unique_ptr<node> root = nullptr;
		std::intptr_t size = 0;
	};

	//static_assert(gorking_list<int>(std::views::iota(-2, 100)).contains(-1));
} // namespace ecs::detail

#endif // !ECS_DETAIL_GORKING_LIST_H

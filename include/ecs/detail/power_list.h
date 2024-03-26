#ifndef ECS_DETAIL_GORKING_LIST_H
#define ECS_DETAIL_GORKING_LIST_H

#include "contract.h"
#include <algorithm>
#include <memory>
#include <queue>
#include <ranges>

namespace ecs::detail {
	template <typename T>
	class power_list {
		struct node {
			node* next[2]{};
			T data{};
		};

		struct balance_helper {
			struct stepper {
				std::size_t target;
				std::size_t size;
				node* from;
				constexpr bool operator<(stepper const& a) const {
					return (a.target < target);
				}
			};

			node* curr{};
			node* last{};
			std::size_t log_n{};
			std::size_t index{0};
			std::array<stepper, 32> steppers{};

			constexpr balance_helper(node* n, std::size_t count) : curr(n), log_n(std::bit_width(count)) {
				Pre(std::cmp_less(log_n, steppers.size()), "List is too large, increase array capacity");

				// Load up steppers
				node* current = n;
				for (std::size_t i = 0; i < log_n; i++) {
					std::size_t const step = std::size_t{1} << (log_n - i);
					steppers[log_n - 1 - i] = {i + step, step, current};
					current = current->next[0];
				}
			}
			constexpr ~balance_helper() {
				while (*this)
					balance_current_and_advance();

				for (std::size_t i = 0; i < log_n; i++)
					steppers[i].from->next[1] = last;
			}
			constexpr void balance_current_and_advance() {
				stepper* min_step = &steppers[log_n - 1];
				while (steppers[0].target == index) {
					std::pop_heap(steppers.data(), steppers.data() + log_n);
					min_step->from->next[1] = curr->next[0];
					min_step->from = curr;
					min_step->target += min_step->size;
					std::push_heap(steppers.data(), steppers.data() + log_n);
				}

				last = curr;
				curr = curr->next[0];
				index += 1;
			}
			constexpr operator bool() const {
				return nullptr != curr;
			}
		};

	public:
		struct iterator {
			friend class power_list;

			// iterator traits
			using difference_type = ptrdiff_t;
			using value_type = T;
			using pointer = const T*;
			using reference = const T&;
			using iterator_category = std::forward_iterator_tag;

			constexpr iterator() noexcept = default;
			constexpr iterator(node* n, std::size_t count) : curr(n) {
				if (count > 0)
					helper = new balance_helper(n, count);
			}
			constexpr iterator(node* curr, node* prev) : curr(curr), prev(prev) {}
			constexpr ~iterator() {
				if (helper)
					delete helper;
			}

			constexpr iterator& operator++() {
				Pre(curr != nullptr, "Trying to step past end of list");
				if (helper)
					helper->balance_current_and_advance();
				prev = curr;
				curr = curr->next[0];
				return *this;
			}

			constexpr iterator operator++(int) {
				iterator const retval = *this;
				++(*this);
				return retval;
			}

			constexpr bool operator==(std::default_sentinel_t) const {
				return curr == nullptr;
			}

			constexpr bool operator==(iterator other) const {
				return curr == other.curr;
			}

			constexpr bool operator!=(iterator other) const {
				return !(*this == other);
			}

			constexpr operator bool() const {
				return curr != nullptr;
			}

			constexpr value_type operator*() const {
				Pre(curr != nullptr, "Dereferencing null");
				return curr->data;
			}

		private:
			node* curr{};
			node* prev{};
			balance_helper* helper{};
		};

		constexpr power_list() = default;
		constexpr power_list(std::ranges::sized_range auto const& range) {
			Pre(std::ranges::is_sorted(range), "Input range must be sorted");

			count = std::size(range);

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

			needs_rebalance = true;
			rebalance();
		}

		constexpr ~power_list() {
			node* n = root;
			while (n) {
				node* next = n->next[0];
				delete n;
				n = next;
			}
		}

		constexpr iterator begin() {
			return {root, static_cast<std::size_t>(needs_rebalance ? count : 0)};
		}

		constexpr std::default_sentinel_t end() {
			return {};
		}

		constexpr std::size_t size() const {
			return count;
		}

		constexpr bool empty() const {
			return nullptr == root;
		}

		constexpr void insert(T val) {
			if (root == nullptr) {
				root = new node{{nullptr, nullptr}, val};
				root->next[1] = root;
			} else if (val < root->data) {
				root = new node{{root, root->next[1]}, val};
			} else if (node* last = root->next[1]; last && val >= last->data) {
				node* n = new node{{nullptr, nullptr}, val};
				last->next[0] = n;
				last->next[1] = n;
				root->next[1] = n;
			} else {
				node* prev = root;
				node* n = root->next[val > root->data];
				while (val > n->data) {
					prev = n;
					n = n->next[val >= n->next[1]->data];
				}

				prev->next[0] = new node{{n, n}, val};
			}

			count += 1;
			needs_rebalance = true;
		}

		constexpr void remove(T val) {
			erase(find_prev(val));
		}

		constexpr void erase(iterator it) {
			if (!it)
				return;

			node* n = it.curr;
			node* next = n->next[0];
			delete n;
			it.prev->next[0] = next;

			count -= 1;
			needs_rebalance = true;
		}

		constexpr void rebalance() {
			if (root && needs_rebalance) {
				balance_helper bh(root, count);
				while (bh)
					bh.balance_current_and_advance();
			}

			needs_rebalance = false;
		}

		constexpr bool contains(T const& val) const {
			return find_prev(val);
		}

	private:
		constexpr struct iterator find_prev(T const& val) const {
			if (root == nullptr || val < root->data || val > root->next[1]->data)
				return {};

			node* prev = nullptr;
			node* n = root;
			while (val > n->data) {
				prev = n;
				n = n->next[val >= n->next[1]->data];
			}

			if (n->data == val)
				return {n, prev};
			else
				return {};
		}

		node* root = nullptr;
		std::size_t count : 63 = 0;
		std::size_t needs_rebalance : 1 = false;
	};

	// UNIT TESTS
	static_assert(
		[] {
			power_list<int> list;
			list.remove(123);
			return list.empty() && list.size() == 0 && !list.contains(0);
		}(),
		"Empty list");

	static_assert(
		[] {
			auto const iota = std::views::iota(-2, 2);
			power_list<int> list(iota);
			for (int v : iota)
				Assert(list.contains(v), "Value not found");
			return true;
		}(),
		"Construction from a range");

	static_assert(
		[] {
			auto const iota = std::views::iota(-2, 2);
			power_list<int> list;
			for (int v : iota)
				list.insert(v);
			for (int v : iota)
				if (!list.contains(v))
					return false;
			return true;
		}(),
		"Insert");

	static_assert(
		[] {
			auto const iota = std::views::iota(-200, 200);
			power_list<int> list;
			for (int v : iota)
				list.insert(v);
			list.rebalance();
			return list.contains(1);
		}(),
		"Explicit rebalance");

	static_assert(
		[] {
			auto const iota = std::views::iota(-200, 200);
			power_list<int> list;
			for (int v : iota)
				list.insert(v);
			for (int v : list)
				v += 0;
			return list.contains(1);
		}(),
		"Implicit rebalance");
} // namespace ecs::detail

#endif // !ECS_DETAIL_GORKING_LIST_H

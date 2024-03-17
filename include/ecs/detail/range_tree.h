#ifndef ECS_DETAIL_RANGE_TREE_H
#define ECS_DETAIL_RANGE_TREE_H

#include "entity_range.h"
#include "contract.h"
#include <coroutine>
#include <vector>
#include <iterator>

namespace ecs::detail {

	// A tree of ranges (technically an interval tree) to be implemented as an AA tree (https://en.wikipedia.org/wiki/AA_tree)
	// Ranges can not overlap in the tree
	// Ranges may be merged iof adjacent, ie. [0,2] and [3,5] may become [0,5]
	// Currently uses a compact layout, can hopefully be optimized to a linear layout (van Emde Boas or Eytzinger)
	class range_tree {
		struct node {
			entity_range range;
			entity_id max = 0;
			int children[2] = {-1, -1};
			int level = 1;
		};

		struct iterator {
			struct promise_type;
			using handle_type = std::coroutine_handle<promise_type>;

			struct promise_type { // required
				entity_range value{0, 0};

				iterator get_return_object() { return iterator(handle_type::from_promise(*this)); }
				std::suspend_never initial_suspend() { return {}; }
				std::suspend_always final_suspend() noexcept { return {}; }
				void unhandled_exception() { std::terminate(); }
				void return_void() {}

				std::suspend_always yield_value(entity_range r) {
					value = r;
					return {};
				}
			};

			handle_type handle;

			iterator(handle_type h) : handle(h) {}
			~iterator() {
				handle.destroy();
			}

			entity_range operator*() const {
				return handle.promise().value;
			}

			bool operator==(std::default_sentinel_t) const {
				return handle.done();
			}
			operator bool() const {
				return !handle.done();
			}

			void operator++() { handle.resume(); }
			void operator++(int) { handle.resume(); }

		private:
		};
		friend iterator;

	public:
		int size() const {
			return static_cast<int>(nodes.size());
		}

		int height() const {
			if (nodes.empty())
				return 0;
			else {
				int h = 1;
				calc_height(root, 1, h);
				return h;
			}
		}

		iterator begin() {
			return iterate(root);
		}

		std::default_sentinel_t end() const {
			return {};
		}

		void insert(entity_range r) {
			PreAudit(!overlaps(r), "can not add range that overlaps with existing range");

			if (nodes.empty()) {
				nodes.emplace_back(r, r.last());
				root = 0;
			} else {
				root = insert(root, r);
			}
		}

		bool overlaps(entity_range r) const {
			if (nodes.empty())
				return false;
			else
				return overlaps(root, r);
		}

	private:
		iterator iterate(int index) {
			if (index != -1) {
				for(auto x = iterate(nodes[index].children[0]); x; ++x)
					co_yield *x;
				co_yield nodes[index].range;
				for (auto x = iterate(nodes[index].children[1]); x; x++)
					co_yield *x;
			}
		}

		int insert(int index, entity_range r) {
			if (index == -1) {
				int const size = static_cast<int>(std::ssize(nodes));
				nodes.emplace_back(r, r.last());
				return size;
			} else {
				bool const left_right = (r.first() >= nodes[index].max);
				nodes[index].children[left_right] = insert(nodes[index].children[left_right], r);
				nodes[index].max = std::max(nodes[index].max, r.last());
				index = skew(index);
				index = split(index);
				return index;
			}
		}

		bool overlaps(int index, entity_range r) const {
			if (index == -1)
				return false;
			else if (nodes[index].range.overlaps(r))
				return true;
			else {
				bool const left_right = (r.first() >= nodes[index].max);
				return overlaps(nodes[index].children[left_right], r);
			}
		}

		void calc_height(int node, int depth, int& h) const {
			if (depth > h)
				h = depth;

			auto const [left, right] = nodes[node].children;
			if (left != -1)
				calc_height(left, depth + 1, h);
			if (right != -1)
				calc_height(right, depth + 1, h);
		}

		int& left(int node) {
			Pre(node != -1, "index must be valid");
			return nodes[node].children[0];
		}

		int& right(int node) {
			Pre(node != -1, "index must be valid");
			return nodes[node].children[1];
		}

		int& level(int node) {
			return nodes[node].level;
		}

		// Removes left-horizontal links
		int skew(int node) {
			if (node == -1)
				return -1;

			int l = left(node);
			if (l == -1)
				return node;

			if (level(l) == level(node)) {
				left(node) = right(l);
				right(l) = node;
				return l;
			}

			return node;
		}

		// Removes consecutive horizontal links
		int split(int node) {
			if (node == -1)
				return -1;

			if (right(node) == -1 || right(right(node)) == -1)
				return node;

			if (level(node) == level(right(right(node)))) {
				int r = right(node);
				right(node) = left(r);
				left(r) = node;
				level(r) += 1;
				return r;
			}

			return node;
		}

	private:
		int root = -1;
		std::vector<node> nodes;
	};
} // namespace ecs::detail

#endif // !ECS_DETAIL_RANGE_TREE_H

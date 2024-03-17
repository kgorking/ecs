#ifndef ECS_DETAIL_RANGE_TREE_H
#define ECS_DETAIL_RANGE_TREE_H

#include "contract.h"
#include "entity_range.h"
#include <coroutine>
#include <iterator>
#include <vector>

namespace ecs::detail {

	// A tree of ranges (technically an interval tree) to be implemented as an AA tree (https://en.wikipedia.org/wiki/AA_tree)
	// Ranges can not overlap in the tree
	// Ranges may be merged iof adjacent, ie. [0,2] and [3,5] may become [0,5]
	// Currently uses a compact layout, can hopefully be optimized to a linear layout (van Emde Boas or Eytzinger)
	class range_tree {
		struct node_t {
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

				iterator get_return_object() {
					return iterator(handle_type::from_promise(*this));
				}
				std::suspend_never initial_suspend() {
					return {};
				}
				std::suspend_always final_suspend() noexcept {
					return {};
				}
				void unhandled_exception() {
					std::terminate();
				}
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

			void operator++() {
				handle.resume();
			}
			void operator++(int) {
				handle.resume();
			}

		private:
		};
		friend iterator;

	public:
		int size() const {
			int s = static_cast<int>(nodes.size());
			int f = free;
			while (f != -1) {
				s -= 1;
				f = nodes[f].level;
			}
			return s;
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

		void insert(entity_range range) {
			PreAudit(!overlaps(range), "can not add range that overlaps with existing range");

			if (nodes.empty()) {
				nodes.emplace_back(range, range.last());
				root = 0;
			} else {
				root = insert(root, range);
			}
		}

		void remove(entity_range range) {
			PreAudit(overlaps(range), "range must overlap existing range");

			if (nodes.empty()) {
				return;
			} else {
				root = remove(root, range);
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
				for (auto x = iterate(nodes[index].children[0]); x; ++x)
					co_yield *x;
				co_yield nodes[index].range;
				for (auto x = iterate(nodes[index].children[1]); x; x++)
					co_yield *x;
			}
		}

		int insert(int i, entity_range range) {
			if (i == -1) {
				return create_node(range);
			} else {
				bool const is_right_node = (range.first() >= nodes[i].max);
				nodes[i].children[is_right_node] = insert(nodes[i].children[is_right_node], range);
				nodes[i].max = std::max(nodes[i].max, range.last());
				i = skew(i);
				i = split(i);
				return i;
			}
		}

		int remove(int i, entity_range range) {
			if (i == -1)
				return -1;

			if (left(i) != -1 && range.overlaps(nodes[left(i)].range)) {
				left(i) = remove(left(i), range);
			}
			if (right(i) != -1 && range.overlaps(nodes[right(i)].range)) {
				right(i) = remove(right(i), range);
			}
			
			if (range.contains(node(i).range)) {
				// The range removes this node whole
				if (leaf(i)) {
					free_node(i);
					return -1;
				} else if (-1 == left(i)) {
					int const s = successor(i);
					right(i) = remove(right(i), node(s).range);
					node(i).range = node(s).range;
				} else {
					int const s = predecessor(i);
					left(i) = remove(left(i), node(s).range);
					node(i).range = node(s).range;
				}
			} else if(range.overlaps(node(i).range)) {
				// it's a partial remove, update the current node
				auto [rng_l, rng_r] = entity_range::remove(node(i).range, range);
				node(i).range = rng_l;
				if (rng_r) {
					// Range was split in two, so add the new range
					right(i) = insert(right(i), *rng_r);
				}
			}

			int const level_l = (-1 == left(i)) ? 1 : level(left(i));
			int const level_r = (-1 == right(i)) ? 1 : level(right(i));
			int const new_level = 1 + std::min(level_l, level_r);
			if (new_level < level(i)) {
				level(i) = new_level;
				if (-1 != right(i) && new_level < level(right(i)))
					level(right(i)) = new_level;
			}

			i = skew(i);
			right(i) = skew(right(i));
			if (-1 != right(i))
				right(right(i)) = skew(right(right(i)));
			i = split(i);
			right(i) = split(right(i));
			return i;
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

		int create_node(entity_range range) {
			if (free == -1) {
				int const size = static_cast<int>(std::ssize(nodes));
				nodes.emplace_back(range, range.last());
				return size;
			} else {
				int const i = free;
				free = nodes[free].level;
				nodes[i] = {range, range.last()};
				return i;
			}
		}

		void free_node(int i) {
			if (i == -1)
				return;
			nodes[i].level = free;
			free = i;
		}

		node_t& node(int i) {
			return nodes[i];
		}

		bool leaf(int i) const {
			return nodes[i].children[0] == -1 && nodes[i].children[1] == -1;
		}

		int successor(int i) {
			i = right(i);
			while (left(i) != -1)
				i = left(i);
			return i;
		}

		int predecessor(int i) {
			i = left(i);
			while (right(i) != -1)
				i = right(i);
			return i;
		}

		int& left(int i) {
			Pre(i != -1, "index must be valid");
			return nodes[i].children[0];
		}

		int& right(int i) {
			Pre(i != -1, "index must be valid");
			return nodes[i].children[1];
		}

		int& level(int i) {
			Pre(i != -1, "index must be valid");
			return nodes[i].level;
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
		int split(int i) {
			if (i == -1)
				return -1;

			if (right(i) == -1 || right(right(i)) == -1)
				return i;

			if (level(i) == level(right(right(i)))) {
				int r = right(i);
				right(i) = left(r);
				left(r) = i;
				level(r) += 1;
				return r;
			}

			return i;
		}

	private:
		int root = -1;
		int free = -1; // free list.
		std::vector<node_t> nodes;
	};
} // namespace ecs::detail

#endif // !ECS_DETAIL_RANGE_TREE_H

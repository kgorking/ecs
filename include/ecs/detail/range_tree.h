#ifndef ECS_DETAIL_RANGE_TREE_H
#define ECS_DETAIL_RANGE_TREE_H

#include "entity_range.h"
#include <coroutine>
#include <vector>
#include <iterator>

namespace ecs::detail {
	class range_tree {
		struct node {
			entity_range range;
			entity_id max = 0;
			int children[2] = {-1, -1};
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
		std::size_t size() const {
			return nodes.size();
		}

		iterator begin() {
			return iterate(0);
		}

		auto end() const {
			return std::default_sentinel;
		}

		void insert(entity_range r) {
			if (nodes.empty()) {
				nodes.emplace_back(r, r.last());
			} else {
				insert(0, r);
			}
		}

		bool overlaps(entity_range r) const {
			if (nodes.empty())
				return false;
			else
				return overlaps(0, r);
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

	private:
		std::vector<node> nodes;
	};
} // namespace ecs::detail

#endif // !ECS_DETAIL_RANGE_TREE_H

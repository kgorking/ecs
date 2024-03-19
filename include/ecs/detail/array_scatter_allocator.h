#ifndef ECS_DETAIL_ARRAY_SCATTER_ALLOCATOR_H
#define ECS_DETAIL_ARRAY_SCATTER_ALLOCATOR_H

#include <memory>
#include <cstdlib>
#include <forward_list>

namespace ecs::detail {
	template<typename T>
	struct array_scatter_allocator {
		array_scatter_allocator() {
			add_pool(16);
		}

		~array_scatter_allocator() {
			for (pool& p : pools) {
				std::destroy_n(p.base, p.capacity);
				std::free(p.base);
			}
		}

		template<typename ...Ts>
		int allocate(int count, auto&& alloc_callback, Ts&& ...args) {
			int splits = 0;
			auto it = pools.begin();
			while(count > 0) {
				if (it == pools.end()) {
					add_pool(pools.front().capacity << 1);
					it = pools.begin();
				}

				pool& p = *it++;

				int const min_space = std::min(count, p.unused);
				if (min_space == 0)
					continue;

				splits += 1;
				count -= min_space;
				p.unused -= min_space;

				T* ptr = p.base + p.next_available;
				p.next_available += min_space;

				for(int i=0; i<min_space; i++)
					std::construct_at(ptr + i, std::forward<Ts>(args)...);
				alloc_callback(ptr, min_space);
			}
			return splits;
		}

		void deallocate(T* ptr, int count) {
			std::destroy_n(ptr, count);
			free_list.emplace_front(ptr, count);
		}

	private:
		void add_pool(int size) {
			pools.emplace_front(size, size, (T*)std::calloc(size, sizeof(T)), 0);
		}

		struct free_block {
			T* ptr;
			int count;
		};

		struct pool {
			int unused;
			int capacity;
			T* base;
			int next_available;
		};

		std::forward_list<pool> pools;
		std::forward_list<free_block> free_list;
	};
}

#endif // !ECS_DETAIL_ARRAY_SCATTER_ALLOCATOR_H

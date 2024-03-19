#ifndef ECS_DETAIL_ARRAY_SCATTER_ALLOCATOR_H
#define ECS_DETAIL_ARRAY_SCATTER_ALLOCATOR_H

#include "contract.h"
#include <memory>
#include <span>

namespace ecs::detail {
	template <typename Fn, typename T>
	concept callback_takes_a_span = std::invocable<Fn, std::span<T>>;

	// the 'Array Scatter Allocator'.
	// * Optimized for allocating arrays of objects, instead of one-at-the-time
	//   that the stl allocators do.
	// * A single allocation can result in many addresses being returned, as the
	//   allocator fills in holes in the internal pools of memory.
	template <typename T, int DefaultStartingSize = 16>
	struct array_scatter_allocator {
		static_assert(DefaultStartingSize > 0);

		template <typename... Ts>
		constexpr std::vector<std::span<T>> allocate(int const count, Ts&&... args) {
			std::vector<std::span<T>> r;
			allocate_with_callback(
				count,
				[&r](std::span<T> s) {
					r.push_back(s);
				},
				std::forward<Ts>(args)...);
			return r;
		}

		template <typename... Ts>
		constexpr int allocate_with_callback(int const count, callback_takes_a_span<T> auto&& alloc_callback, Ts&&... args) {
			int splits = 0;
			int remaining_count = count;

			// Take space from free list
			std::unique_ptr<free_block>* ptr_free = &free_list;
			while (remaining_count > 0 && *ptr_free) {
				free_block* ptr = ptr_free->get();
				int const min_space = std::min(remaining_count, (int)(ptr->span.size()));
				if (min_space == 0) {
					ptr_free = &ptr->next;
					continue;
				}

				std::span<T> span = ptr->span.subspan(0, min_space);
				for (T& val : span)
					val = {args...};

				splits += 1;
				remaining_count -= min_space;

				if (min_space == (int)ptr->span.size()) {
					*ptr_free = std::move(ptr->next);
				} else {
					ptr->span = ptr->span.subspan(min_space + 1);
					ptr_free = &ptr->next;
				}
			}

			// Take space from pool(s)
			pool* ptr_pool = pools.get();
			while (remaining_count > 0) {
				if (ptr_pool == nullptr)
					ptr_pool = add_pool(pools ? pools->capacity << 1 : DefaultStartingSize);

				pool& p = *ptr_pool;
				ptr_pool = ptr_pool->next.get();

				unsigned const min_space = std::min(remaining_count, p.unused);
				if (min_space == 0)
					continue;

				splits += 1;
				remaining_count -= min_space;
				p.unused -= min_space;

				T* ptr = p.base.get() + p.next_available;
				p.next_available += min_space;

				for (unsigned i = 0; i < min_space; i++)
					ptr[i] = {args...};

				alloc_callback(std::span<T>{ptr, min_space});
			}

			return splits;
		}

		constexpr void deallocate(std::span<T> const span) {
			PreAudit(validate_addr(span), "Invalid address passed to deallocate()");
			free_list = std::make_unique<free_block>(span, std::move(free_list));
		}

	private:
		constexpr auto* add_pool(int const size) {
			pools = std::make_unique<pool>(std::move(pools), std::make_unique_for_overwrite<T[]>(size), size, size, 0);
			return pools.get();
		}

		constexpr bool validate_addr(std::span<T> const span) {
			auto* p = pools.get();
			while (p != nullptr) {
				T const* const begin = p->base.get();
				T const* const end = begin + p->capacity;
				if (span.data() >= begin && (span.data() + span.size()) < end)
					return true;
				p = p->next.get();
			}
			return false;
		}

		struct free_block {
			std::span<T> span;
			std::unique_ptr<free_block> next;
		};

		struct pool {
			std::unique_ptr<pool> next;
			std::unique_ptr<T[]> base;
			int unused;
			int capacity;
			int next_available;
		};

		std::unique_ptr<pool> pools;
		std::unique_ptr<free_block> free_list;
	};

	// UNIT TESTS
	static_assert(
		[] {
			constexpr std::size_t elems_to_alloc = 12'345;
			array_scatter_allocator<int> alloc;
			std::size_t total_alloc = 0;
			alloc.allocate_with_callback(elems_to_alloc, [&](std::span<int> s) {
				total_alloc += s.size();
			});
			return elems_to_alloc == total_alloc;
		}(),
		"Array-scatter allocator allocates correctly");

	static_assert(
		[] {
			array_scatter_allocator<int> alloc;

			int sum = 0;
			alloc.allocate_with_callback(
				10,
				[&](std::span<int> s) {
					for (int val : s)
						sum += val;
				},
				5);
			return 50 == sum;
		}(),
		"Array-scatter allocator initializes correctly");

	static_assert(
		[] {
			array_scatter_allocator<int> alloc;
			std::vector<std::span<int>> r = alloc.allocate(10);
			auto const subspan = r[0].subspan(3, 4);
			alloc.deallocate(subspan);
			return true;
		},
		"Array-scatter allocator frees correctly");

	static_assert(
		[] {
			array_scatter_allocator<int> alloc;
			auto vec = alloc.allocate(10);			// +10
			alloc.deallocate(vec[0].subspan(3, 4)); // -4
			vec = alloc.allocate(20);				// +20
			return true;
		},
		"Array-scatter allocator scatters correctly");
} // namespace ecs::detail

#endif // !ECS_DETAIL_ARRAY_SCATTER_ALLOCATOR_H

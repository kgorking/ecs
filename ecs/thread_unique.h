#pragma once
#include <mutex>
#include <vector>
#include <list>
#include <memory>
#include <cassert>
#include <future>

namespace ecs
{
	// Creates a type T for each thread that accesses it
	template <typename T>
	class thread_unique
	{
		struct thread
		{
			using thread_data = std::pair<thread_unique<T>*, T*>;
			std::list<thread_data> accessors;

			~thread()
			{
				/*for (auto &td : accessors) {
					if (td.second.use_count() > 1)
						td.first->remove_thread(this);
				}*/
			}

			T& get(thread_unique<T>* accessor)
			{
				auto it = std::find_if(accessors.begin(), accessors.end(), [accessor](auto &acc) { return accessor == acc.first; });
				if (it != accessors.end()) {
					return *((*it).second);
				}

				T* wp = accessor->init_thread(this);
				accessors.push_front(thread_data{ accessor, wp });
				return *wp;
			}

			void remove(thread_unique<T>* accessor)
			{
				accessors.remove_if([accessor](auto &acc) { return accessor == acc.first; });
			}
		};

		friend thread;

		// Adds a thread and allocates its data.
		// Returns a weak pointer to the data
		T* init_thread(thread *t)
		{
			std::scoped_lock sl(mtx_storage);
			threads.push_back(t);
			return &locals.emplace_front();
		}

		// Removes data from dead threads
		void remove_thread(thread *t)
		{
			// Zero out the thread in the vector. It, and its data, is removed on reduce.
			for (thread *& val : threads) {
				if (val == t) {
					val = nullptr;
					break;
				}
			}
		}

		void cleanup()
		{
			// Do some cleanup
			size_t offset = 0;
			auto last_loc = std::remove_if(locals.begin(), locals.end(), [this, &offset](T &) {
				return *(threads.data() + offset++) == nullptr;
			});
			auto last_thr = std::remove_if(threads.begin(), threads.end(), [](thread *t) {
				return t == nullptr;
			});
			locals.erase(last_loc, locals.end());
			threads.erase(last_thr, threads.end());
		}

		template <typename RandomIt, class Reducer>
		static auto parallel_reduce(RandomIt beg, RandomIt end, Reducer reduce_op)
		{
			if (beg == end)
				return *beg;

			auto len = std::distance(beg, end);

			RandomIt mid = std::next(beg, len / 2);
			auto handle = std::async(std::launch::async, parallel_reduce<RandomIt, Reducer>, std::next(mid), end, reduce_op);
			auto sum = parallel_reduce(beg, mid, reduce_op);
			return reduce_op((sum), (handle.get()));
		}

	public:
		// Get the instance of T for the current thread
		T& get()
		{
			thread_local thread var;
			return var.get(this);
		}

		void clear()
		{
			for (T &t : locals)
				t = T();
		}

		template <class Function>
		void for_each(Function f)
		{
			for (T &t : locals)
				f(t);
		}

		// reduce_op = T(T a, T b) { return a + b; }
		template <class BinaryOp>
		T reduce(BinaryOp reduce_op)
		{
			// Do the reduction
			T val;
			val = std::reduce(std::execution::par, locals.begin(), locals.end(), std::move(val), reduce_op);
			/*T val;
			if (!locals.empty())
				val = parallel_reduce(locals.begin(), std::prev(locals.end()), reduce_op);*/

			cleanup();

			return val;
		}

		// Overload to get access to the current threads instance of T
		T* operator ->()
		{
			return &get();
		}
		T const* operator ->() const
		{
			return &const_cast<thread_unique<T>*>(this)->get();
		}

	private:
		// the threads that access this object
		std::vector<thread*> threads;

		// local data held by each thread
		std::list<T> locals;

		// Mutex for serializing access for creating/removing locals
		std::mutex mtx_storage;
	};
}

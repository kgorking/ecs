#pragma once
#include <mutex>
#include <vector>
#include <list>
#include <algorithm>

// Provides a thread-local instance of the type T for each thread that
// accesses it. The set of instances can be accessed through the begin()/end() iterators.
template <typename T>
class threaded
{
	// This struct manages the instance that access the thread-local data.
	// Its lifetime is marked as thread_local, which means that it can live longer than
	// the threaded<> instance that spawned it.
	struct instance_data
	{
		// Return this threads local data
		T& get(threaded<T> &instance)
		{
			if (&instance != owner) {
				owner = &instance;
				data = instance.init_thread(*this);
			}

			return *data;
		}

		void clear() noexcept
		{
			owner = nullptr;
			data = nullptr;
		}

	private:
		threaded<T>* owner{};
		T* data{};
	};
	friend instance_data;

	// Adds a instance_data and allocates its instance.
	// Returns a pointer to the instance
	T* init_thread(instance_data & t)
	{
		// Mutex for serializing access for creating/removing locals
		static std::mutex mtx_storage;
		std::scoped_lock sl(mtx_storage);

		threads.push_back(&t);
		return &data.emplace_front();
	}

public:
	~threaded() noexcept
	{
		clear();
	}

	auto begin() noexcept { return data.begin(); }
	auto end() noexcept { return data.end(); }

	// Get the thread-local instance of T for the current thread
	T& local()
	{
		thread_local instance_data var{};
		return var.get(*this);
	}

	// Clears all the thread instances
	void clear() noexcept
	{
		for (auto thread : threads) {
			if (thread != nullptr)
				thread->clear();
		}

		threads.clear();
		data.clear();
	}

private:
	// the threads that access this intance
	std::vector<instance_data*> threads;

	// instance-data created by each thread. list contents are not invalidated when more items are added, unlike a vector
	std::list<T> data;
};

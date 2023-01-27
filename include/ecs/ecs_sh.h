// Auto-generated single-header include file
#if 0 //defined(__has_cpp_attribute) && __has_cpp_attribute(__cpp_lib_modules)
import std;
#else
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <execution>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <mutex> // needed for scoped_lock
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#endif


#ifndef TLS_CACHE
#define TLS_CACHE


namespace tls {
// A class using a cache-line to cache data.
template <class Key, class Value, Key empty_slot = Key{}, size_t cache_line = 64UL>
class cache {
	// If you trigger this assert, then either your key- or value size is too large,
	// or you cache_line size is too small.
	// The cache should be able to hold at least 4 key/value pairs in order to be efficient.
	static_assert((sizeof(Key) + sizeof(Value)) <= (cache_line / 4), "key or value size too large");

	static constexpr size_t num_entries = (cache_line) / (sizeof(Key) + sizeof(Value));

public:
	constexpr cache() {
		reset();
	}

	// Returns the value if it exists in the cache,
	// otherwise inserts 'or_fn(k)' in cache and returns it
	template <class Fn>
	constexpr Value get_or(Key const k, Fn or_fn) {
		size_t index = num_entries;
		for (size_t i = 0; i < num_entries; i++) {
			// no break generates cmov's instead of jumps
			if (k == keys[i])
				index = i;
		}
		if (index != num_entries)
			return values[index];

		insert_val(k, or_fn(k));
		return values[0];
	}

	// Clears the cache
	constexpr void reset() {
		std::fill(keys, keys + num_entries, empty_slot);
		std::fill(values, values + num_entries, Value{});
	}

	// Returns the number of key/value pairs that can be cached
	static constexpr size_t max_entries() {
		return num_entries;
	}

protected:
	constexpr void insert_val(Key const k, Value const v) {
		// Move all pairs one step to the right
		std::shift_right(keys, keys + num_entries, 1);
		std::shift_right(values, values + num_entries, 1);

		// Insert the new pair at the front of the cache
		keys[0] = k;
		values[0] = v;
	}

private:
	Key keys[num_entries];
	Value values[num_entries];
};

} // namespace tls

#endif // !TLS_CACHE
#ifndef TLS_SPLIT_H
#define TLS_SPLIT_H


namespace tls {
// Provides a thread-local instance of the type T for each thread that
// accesses it. Data is not preserved when threads die.
// This class locks when a thread is created/destroyed.
// The thread_local T's can be accessed through split::for_each.
// Note: Two split<T> instances in the same thread will point to the same data.
//       Differentiate between them by passing different types to 'UnusedDifferentiaterType'.
//       As the name implies, it's not used internally, so just put whatever.
template <typename T, typename UnusedDifferentiaterType = void>
class split {
	// This struct manages the instances that access the thread-local data.
	// Its lifetime is marked as thread_local, which means that it can live longer than
	// the split<> instance that spawned it.
	struct thread_data {
		// The destructor triggers when a thread dies and the thread_local
		// instance is destroyed
		~thread_data() noexcept {
			if (owner != nullptr) {
				owner->remove_thread(this);
			}
		}

		// Return a reference to an instances local data
		[[nodiscard]] T& get(split* instance) noexcept {
			// If the owner is null, (re-)initialize the instance.
			// Data may still be present if the thread_local instance is still active
			if (owner == nullptr) {
				data = {};
				owner = instance;
				instance->init_thread(this);
			}
			return data;
		}

		void remove(split* instance) noexcept {
			if (owner == instance) {
				data = {};
				owner = nullptr;
				next = nullptr;
			}
		}

		[[nodiscard]] T* get_data() noexcept {
			return &data;
		}
		[[nodiscard]] T const* get_data() const noexcept {
			return &data;
		}

		void set_next(thread_data* ia) noexcept {
			next = ia;
		}
		[[nodiscard]] thread_data* get_next() noexcept {
			return next;
		}
		[[nodiscard]] thread_data const* get_next() const noexcept {
			return next;
		}

	private:
		T data{};
		split<T, UnusedDifferentiaterType> *owner{};
		thread_data *next = nullptr;
	};

private:
	// the head of the threads that access this split instance
	inline static thread_data* head{};

	// Mutex for serializing access for adding/removing thread-local instances
	inline static std::shared_mutex mtx;

protected:
	// Adds a thread_data
	void init_thread(thread_data* t) noexcept {
		std::scoped_lock sl(mtx);
		t->set_next(head);
		head = t;
	}

	// Remove the thread_data
	void remove_thread(thread_data* t) noexcept {
		std::scoped_lock sl(mtx);
		// Remove the thread from the linked list
		if (head == t) {
			head = t->get_next();
		} else {
			auto curr = head;
			while (curr->get_next() != nullptr) {
				if (curr->get_next() == t) {
					curr->set_next(t->get_next());
					return;
				} else {
					curr = curr->get_next();
				}
			}
		}
	}

public:
	split() noexcept = default;
	split(split const&) = delete;
	split(split&&) noexcept = default;
	split& operator=(split const&) = delete;
	split& operator=(split&&) noexcept = default;
	~split() noexcept {
		reset();
	}

	// Get the thread-local instance of T
	T& local() noexcept {
		thread_local thread_data var{};
		return var.get(this);
	}

	// Performa an action on all each instance of the data
	template<class Fn>
	void for_each(Fn&& fn) {
		std::scoped_lock sl(mtx);
		for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
			fn(*thread->get_data());
		}
	}

	// Resets all data and threads
	void reset() noexcept {
		std::scoped_lock sl(mtx);
		for (thread_data* instance = head; instance != nullptr;) {
			auto next = instance->get_next();
			instance->remove(this);
			instance = next;
		}

		head = nullptr;
	}
};
} // namespace tls

#endif // !TLS_SPLIT_H
#ifndef TLS_COLLECT_H
#define TLS_COLLECT_H


namespace tls {
// Works like tls::split, except data is preserved when threads die.
// You can collect the thread_local T's with collect::gather,
// which also resets the data on the threads by moving it.
// Note: Two runtime collect<T> instances in the same thread will point to the same data.
//       If they are evaluated in a constant context each instance has their own data.
//       Differentiate between runtime versions by passing different types to 'UnusedDifferentiaterType',
// 	     so fx collect<int, struct A> is different from collect<int, struct B>.
//       As the name implies, 'UnusedDifferentiaterType' is not used internally, so just put whatever.
template <typename T, typename UnusedDifferentiaterType = void>
class collect {
	// This struct manages the instances that access the thread-local data.
	// Its lifetime is marked as thread_local, which means that it can live longer than
	// the splitter<> instance that spawned it.
	struct thread_data {
		~thread_data() noexcept {
			if (owner != nullptr) {
				owner->remove_thread(this);
			}
		}

		// Return a reference to an instances local data
		[[nodiscard]] T& get(collect* instance) noexcept {
			// If the owner is null, (re-)initialize the thread.
			// Data may still be present if the thread_local instance is still active
			if (owner == nullptr) {
				data = {};
				owner = instance;
				instance->init_thread(this);
			}
			return data;
		}

		void remove(collect* instance) noexcept {
			if (owner == instance) {
				data = {};
				owner = nullptr;
				next = nullptr;
			}
		}

		[[nodiscard]] T* get_data() noexcept {
			return &data;
		}
		[[nodiscard]] T const* get_data() const noexcept {
			return &data;
		}

		void set_next(thread_data* ia) noexcept {
			next = ia;
		}
		[[nodiscard]] thread_data* get_next() noexcept {
			return next;
		}
		[[nodiscard]] thread_data const* get_next() const noexcept {
			return next;
		}

	private:
		T data{};
		collect* owner{};
		thread_data* next = nullptr;
	};

private:
	// the head of the threads that access this collect instance
	inline static thread_data* head{};

	// All the data collected from threads
	inline static std::vector<T> data{};

	// Mutex for serializing access for adding/removing thread-local instances
	inline static std::shared_mutex mtx;

	// Adds a new thread
	void init_thread(thread_data* t) noexcept {
		std::scoped_lock sl(mtx);
		t->set_next(head);
		head = t;
	}

	// Removes the thread
	void remove_thread(thread_data* t) noexcept {
		std::scoped_lock sl(mtx);

		// Take the thread data
		T* local_data = t->get_data();
		data.push_back(static_cast<T&&>(*local_data));

		// Reset the thread data
		*local_data = T{};

		// Remove the thread from the linked list
		if (head == t) {
			head = t->get_next();
		} else {
			auto curr = head;
			while (curr->get_next() != nullptr) {
				if (curr->get_next() == t) {
					curr->set_next(t->get_next());
					return;
				} else {
					curr = curr->get_next();
				}
			}
		}
	}

public:
	collect() noexcept = default;
	collect(collect const&) = delete;
	collect(collect&&) noexcept = default;
	collect& operator=(collect const&) = delete;
	collect& operator=(collect&&) noexcept = default;
	~collect() noexcept {
		reset();
	}

	// Get the thread-local thread of T
	[[nodiscard]] T& local() noexcept {
		thread_local thread_data var{};
		return var.get(this);
	}

	// Gathers all the threads data and returns it. This clears all stored data.
	[[nodiscard]] std::vector<T> gather() noexcept {
		std::scoped_lock sl(mtx);
		for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
			data.push_back(std::move(*thread->get_data()));
			*thread->get_data() = T{};
		}

		return static_cast<std::vector<T>&&>(data);
	}

	// Gathers all the threads data and sends it to the output iterator. This clears all stored data.
	void gather_flattened(auto dest_iterator) noexcept {
		std::scoped_lock sl(mtx);
		using U = typename T::value_type;

		for (T& per_thread_data : data) {
			//std::move(t.begin(), t.end(), dest_iterator);
			for (U& elem : per_thread_data) {
				*dest_iterator = static_cast<U&&>(elem);
				++dest_iterator;
			}
		}
		data.clear();

		for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
			T* ptr_per_thread_data = thread->get_data();
			//std::move(ptr_per_thread_data->begin(), ptr_per_thread_data->end(), dest_iterator);
			for (U& elem : *ptr_per_thread_data) {
				*dest_iterator = static_cast<U&&>(elem);
				++dest_iterator;
			}
			//*ptr_per_thread_data = T{};
			ptr_per_thread_data->clear();
		}
	}

	// Perform an action on all threads data
	template <class Fn>
	void for_each(Fn&& fn) noexcept {
		std::scoped_lock sl(mtx);
		for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
			fn(*thread->get_data());
		}

		for (auto& d : data)
			fn(d);
	}

	// Perform an non-modifying action on all threads data
	template <class Fn>
	void for_each(Fn&& fn) const noexcept {
		{
			std::scoped_lock sl(mtx);
			for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
				fn(*thread->get_data());
			}
		}

		for (auto const& d : data)
			fn(d);
	}

	// Resets all data and threads
	void reset() noexcept {
		std::scoped_lock sl(mtx);
		for (thread_data* thread = head; thread != nullptr;) {
			auto next = thread->get_next();
			thread->remove(this);
			thread = next;
		}

		head = nullptr;
		data.clear();
	}
};
} // namespace tls

#endif // !TLS_COLLECT_H
#ifndef TYPE_LIST_H_
#define TYPE_LIST_H_

namespace ecs::detail {

//
// Helper templates for working with types at compile-time

// inspired by https://devblogs.microsoft.com/cppblog/build-throughput-series-more-efficient-template-metaprogramming/

template <typename...>
struct type_list;

template <typename First, typename Second>
struct type_pair {
	using first = First;
	using second = Second;
};

namespace impl {
	//
	// detect type_list
	template <typename... Types>
	consteval bool detect_type_list(type_list<Types...>*) {
		return true;
	}
	consteval bool detect_type_list(...) {
		return false;
	}


	//
	// type_list_size.
	template <typename> struct type_list_size;
	template <typename... Types> struct type_list_size<type_list<Types...>> {
		static constexpr size_t value = sizeof...(Types);
	};


	//
	// type_list indices
	template<int Index, typename...>
	struct type_list_index {
		using type_not_found_in_list = decltype([]{});
		consteval static int index_of(type_not_found_in_list*);
	};

	template<int Index, typename T, typename... Rest>
	struct type_list_index<Index, T, Rest...> : type_list_index<1+Index, Rest...> {
		using type_list_index<1+Index, Rest...>::index_of;

		consteval static int index_of(T*) {
			return Index;
		}
	};

	template<typename... Types>
	consteval auto type_list_indices(type_list<Types...>*) {
		struct all_indexers : impl::type_list_index<0, Types...> {
			using impl::type_list_index<0, Types...>::index_of;
		};
		return all_indexers{};
	}

	//
	// type_list concept
	template <typename TL>
	concept TypeList = detect_type_list(static_cast<TL*>(nullptr));


	//
	// helper functions
	template <typename... Types, typename F>
	constexpr void for_each_type(F&& f, type_list<Types...>*) {
		(f.template operator()<Types>(), ...);
	}

	template <typename T, typename... Types, typename F>
	constexpr void for_specific_type(F&& f, type_list<Types...>*) {
		auto const runner = [&f]<typename X>() {
			if constexpr (std::is_same_v<T, X>) {
				f();
			}
		};

		(runner.template operator()<Types>(), ...);
	}

	template <typename T, typename... Types, typename F, typename NF>
	constexpr void for_specific_type_or(F&& f, NF&& nf, type_list<Types...>*) {
		auto const runner = [&]<typename X>() {
			if constexpr (std::is_same_v<T, X>) {
				f();
			} else {
				nf();
			}
		};

		(runner.template operator()<Types>(), ...);
	}

	template <typename... Types, typename F>
	constexpr decltype(auto) for_all_types(F&& f, type_list<Types...>*) {
		return f.template operator()<Types...>();
	}

	template <typename... Types, typename F>
	constexpr bool all_of_type(F&& f, type_list<Types...>*) {
		return (f.template operator()<Types>() && ...);
	}

	template <typename... Types, typename F>
	constexpr bool any_of_type(F&& f, type_list<Types...>*) {
		return (f.template operator()<Types>() || ...);
	}

	template <template <typename O> typename Tester, typename... Types, typename F>
	constexpr auto run_if(F&& /*f*/, type_list<>*) {
		return;
	}

	template <template <typename O> typename Tester, typename FirstType, typename... Types, typename F>
	constexpr auto run_if(F&& f, type_list<FirstType, Types...>*) {
		if constexpr (Tester<FirstType>::value) {
			return f.template operator()<FirstType>();
		} else {
			return run_if<Tester, Types...>(f, static_cast<type_list<Types...>*>(nullptr));
		}
	}

	template <typename... Types, typename F>
	constexpr std::size_t count_type_if(F&& f, type_list<Types...>*) {
		return (static_cast<std::size_t>(f.template operator()<Types>()) + ...);
	}

	
	template <typename TL, template <typename O> typename Transformer>
	struct transform_type {
		template <typename... Types>
		constexpr static type_list<Transformer<Types>...>* helper(type_list<Types...>*);

		using type = std::remove_pointer_t<decltype(helper(static_cast<TL*>(nullptr)))>;
	};
	
	template <typename TL, template <typename... O> typename Transformer>
	struct transform_type_all {
		template <typename... Types>
		constexpr static Transformer<Types...>* helper(type_list<Types...>*);

		using type = std::remove_pointer_t<decltype(helper(static_cast<TL*>(nullptr)))>;
	};

	template <typename TL, typename T>
	struct add_type {
		template <typename... Types>
		constexpr static type_list<Types..., T>* helper(type_list<Types...>*);

		using type = std::remove_pointer_t<decltype(helper(static_cast<TL*>(nullptr)))>;
	};
	
	template <typename TL, template <typename O> typename Predicate>
	struct split_types_if {
		template <typename ListTrue, typename ListFalse, typename Front, typename... Rest >
		constexpr static auto helper(type_list<Front, Rest...>*) {
			if constexpr (Predicate<Front>::value) {
				using NewListTrue = typename add_type<ListTrue, Front>::type;

				if constexpr (sizeof...(Rest) > 0) {
					return helper<NewListTrue, ListFalse>(static_cast<type_list<Rest...>*>(nullptr));
				} else {
					return static_cast<type_pair<NewListTrue, ListFalse>*>(nullptr);
				}
			} else {
				using NewListFalse = typename add_type<ListFalse, Front>::type;

				if constexpr (sizeof...(Rest) > 0) {
					return helper<ListTrue, NewListFalse>(static_cast<type_list<Rest...>*>(nullptr));
				} else {
					return static_cast<type_pair<ListTrue, NewListFalse>*>(nullptr);
				}
			}
		}

		using list_pair = 
			std::remove_pointer_t<decltype(helper<type_list<>, type_list<>>(static_cast<TL*>(nullptr)))>;
	};

	template<typename First, typename... Types>
	constexpr bool is_unique_types(type_list<First, Types...>*) {
		if constexpr ((std::is_same_v<First, Types> || ...))
			return false;
		else {
			if constexpr (sizeof...(Types) == 0)
				return true;
			else
				return is_unique_types<Types...>({});
		}
	}

	template <typename T, typename... Types>
	static constexpr bool contains_type(type_list<Types...>*) {
		return (std::is_same_v<T, Types> || ...);
	}

	template <typename... Types1, typename... Types2>
	constexpr auto concat_type_lists(type_list<Types1...>*, type_list<Types2...>*)
	-> type_list<Types1..., Types2...>*;

	struct merger {
		template <typename... Left>
		static auto helper(type_list<Left...>*, type_list<>*)
		-> type_list<Left...>*;

	#if defined(_MSC_VER) && !defined(__clang__)
		template <typename... Left, typename FirstRight, typename... Right>
		static auto helper(type_list<Left...>*, type_list<FirstRight, Right...>*)
		-> decltype(merger::helper(
			static_cast<type_list<Left...>*>(nullptr),
			static_cast<type_list<Right...>*>(nullptr)));

		template <typename... Left, typename FirstRight, typename... Right>
		static auto helper(type_list<Left...>*, type_list<FirstRight, Right...>*)
		-> decltype(merger::helper(
			static_cast<type_list<Left..., FirstRight>*>(nullptr),
			static_cast<type_list<Right...>*>(nullptr)))
		requires(!contains_type<FirstRight>(static_cast<type_list<Left...>*>(nullptr)));
	#else
		template <typename... Types1, typename First2, typename... Types2>
		constexpr static auto* helper(type_list<Types1...>*, type_list<First2, Types2...>*) {
			using NewTL2 = type_list<Types2...>;

			if constexpr (contains_type<First2>(static_cast<type_list<Types1...>*>(nullptr))) {
				if constexpr(sizeof...(Types2) == 0)
					return static_cast<type_list<Types1...>*>(nullptr);
				else
					return helper(static_cast<type_list<Types1...>*>(nullptr), static_cast<NewTL2*>(nullptr));
			} else {
				if constexpr(sizeof...(Types2) == 0)
					return static_cast<type_list<Types1..., First2>*>(nullptr);
				else
					return helper(static_cast<type_list<Types1..., First2>*>(nullptr), static_cast<NewTL2*>(nullptr));
			}
		}
	#endif
	};

} // namespace impl

template <impl::TypeList TL>
constexpr size_t type_list_size = impl::type_list_size<TL>::value;

// Classes can inherit from type_list_indices with a provided type_list
// to have 'index_of(T*)' functions injected into it, for O(1) lookups
// of the indices of the types in the type_list
template<typename TL>
using type_list_indices = decltype(impl::type_list_indices(static_cast<TL*>(nullptr)));

// Small helper to get the index of a type in a type_list
template <typename T, typename TL>
consteval int index_of() {
	using TLI = type_list_indices<TL>;
	return TLI::index_of(static_cast<T*>(nullptr));
}

// Transforms the types in a type_list
// Takes transformer that results in new type, like remove_cvref_t
template <impl::TypeList TL, template <typename O> typename Transformer>
using transform_type = typename impl::transform_type<TL, Transformer>::type;

// Transforms all the types in a type_list at once
// Takes a transformer that results in a new type
// Ex.
// 	template <typename... Types>
//  using transformer = std::tuple<entity_id, Types...>;
template <impl::TypeList TL, template <typename... O> typename Transformer>
using transform_type_all = typename impl::transform_type_all<TL, Transformer>::type;

// Splits a type_list into two list depending on the predicate
template <impl::TypeList TL, template <typename O> typename Predicate>
using split_types_if = typename impl::split_types_if<TL, Predicate>::list_pair;

// Applies the functor F to each type in the type list.
// Takes lambdas of the form '[]<typename T>() {}'
template <impl::TypeList TL, typename F>
constexpr void for_each_type(F&& f) {
	impl::for_each_type(f, static_cast<TL*>(nullptr));
}

// Applies the functor F to a specific type in the type list.
// Takes lambdas of the form '[]() {}'
template <typename T, impl::TypeList TL, typename F>
constexpr void for_specific_type(F&& f) {
	impl::for_specific_type<T>(f, static_cast<TL*>(nullptr));
}

// Applies the functor F when a specific type in the type list is found,
// applies NF when not found
// Takes lambdas of the form '[]() {}'
template <typename T, impl::TypeList TL, typename F, typename NF>
constexpr void for_specific_type_or(F&& f, NF&& nf) {
	impl::for_specific_type_or<T>(f, nf, static_cast<TL*>(nullptr));
}

// Applies the functor F to all types in the type list.
// Takes lambdas of the form '[]<typename ...T>() {}'
template <impl::TypeList TL, typename F>
constexpr decltype(auto) for_all_types(F&& f) {
	return impl::for_all_types(f, static_cast<TL*>(nullptr));
}

// Applies the bool-returning functor F to each type in the type list.
// Returns true if all of them return true.
// Takes lambdas of the form '[]<typename T>() -> bool {}'
template <impl::TypeList TL, typename F>
constexpr bool all_of_type(F&& f) {
	return impl::all_of_type(f, static_cast<TL*>(nullptr));
}

// Applies the bool-returning functor F to each type in the type list.
// Returns true if any of them return true.
// Takes lambdas of the form '[]<typename T>() -> bool {}'
template <impl::TypeList TL, typename F>
constexpr bool any_of_type(F&& f) {
	return impl::any_of_type(f, static_cast<TL*>(nullptr));
}

// Runs F once when a type satifies the tester. F takes a type template parameter and can return a value.
template <template <typename O> typename Tester, impl::TypeList TL, typename F>
constexpr auto run_if(F&& f) {
	return impl::run_if<Tester>(f, static_cast<TL*>(nullptr));
}

// Returns the count of all types that satisfy the predicate. F takes a type template parameter and returns a boolean.
template <impl::TypeList TL, typename F>
constexpr std::size_t count_type_if(F&& f) {
	return impl::count_type_if(f, static_cast<TL*>(nullptr));
}

// Returns true if all types in the list are unique
template <impl::TypeList TL>
constexpr bool is_unique_types() {
	return impl::is_unique_types(static_cast<TL*>(nullptr));
}

// Returns true if a type list contains the type
template <typename T, impl::TypeList TL>
constexpr bool contains_type() {
	return impl::contains_type<T>(static_cast<TL*>(nullptr));
}

// concatenates two type_list
template <impl::TypeList TL1, impl::TypeList TL2>
using concat_type_lists = std::remove_pointer_t<decltype(
	impl::concat_type_lists(static_cast<TL1*>(nullptr), static_cast<TL2*>(nullptr)))>;

// merge two type_list, duplicate types are ignored
template <typename TL1, typename TL2>
using merge_type_lists = std::remove_pointer_t<decltype(
	impl::merger::helper(static_cast<TL1*>(nullptr), static_cast<TL2*>(nullptr)))>;

} // namespace ecs::detail
#endif // !TYPE_LIST_H_
#ifndef ECS_CONTRACT
#define ECS_CONTRACT

// Contracts. If they are violated, the program is an invalid state, so nuke it from orbit
#define Expects(cond)                                                                                                                      \
	do {                                                                                                                                   \
		((cond) ? static_cast<void>(0) : std::terminate());                                                                                \
	} while (false)
#define Ensures(cond)                                                                                                                      \
	do {                                                                                                                                   \
		((cond) ? static_cast<void>(0) : std::terminate());                                                                                \
	} while (false)

#endif // !ECS_CONTRACT
#ifndef ECS_TYPE_HASH
#define ECS_TYPE_HASH


// Beware of using this with local defined structs/classes
// https://developercommunity.visualstudio.com/content/problem/1010773/-funcsig-missing-data-from-locally-defined-structs.html

namespace ecs::detail {

using type_hash = std::uint64_t;

template <typename T>
consteval auto get_type_name() {
#ifdef _MSC_VER
	std::string_view fn = __FUNCSIG__;
	auto const type_start = fn.find("get_type_name<") + 14;
	auto const type_end = fn.rfind(">(void)");
	return fn.substr(type_start, type_end - type_start);
#else
	std::string_view fn = __PRETTY_FUNCTION__;
	auto const type_start = fn.rfind("T = ") + 4;
	auto const type_end = fn.rfind("]");
	return fn.substr(type_start, type_end - type_start);
#endif
}

template <typename T>
consteval type_hash get_type_hash() {
	type_hash const prime = 0x100000001b3;
#ifdef _MSC_VER
	std::string_view string = __FUNCDNAME__; // has full type info, but is not very readable
#else
	std::string_view string = __PRETTY_FUNCTION__;
#endif
	type_hash hash = 0xcbf29ce484222325;
	for (char const value : string) {
		hash ^= static_cast<type_hash>(value);
		hash *= prime;
	}

	return hash;
}

template <typename TypesList>
consteval auto get_type_hashes_array() {
	return for_all_types<TypesList>([]<typename... Types>() {
		return std::array<detail::type_hash, sizeof...(Types)>{get_type_hash<Types>()...};
	});
}

} // namespace ecs::detail

#endif // !ECS_TYPE_HASH
#ifndef ECS_DETAIL_TAGGED_POINTER_H
#define ECS_DETAIL_TAGGED_POINTER_H


namespace ecs::detail {

// 1-bit tagged pointer
// Note: tags are considered seperate from the pointer, and is
// therfore not reset when a new pointer is set
template <typename T>
struct tagged_pointer {
	tagged_pointer(T* in) noexcept : ptr(reinterpret_cast<uintptr_t>(in)) {
		Expects((ptr & TagMask) == 0);
	}

	tagged_pointer() noexcept = default;
	tagged_pointer(tagged_pointer const&) noexcept = default;
	tagged_pointer(tagged_pointer&&) noexcept = default;
	tagged_pointer& operator=(tagged_pointer const&) noexcept = default;
	tagged_pointer& operator=(tagged_pointer&&) noexcept = default;

	tagged_pointer& operator=(T* in) noexcept {
		auto const set = ptr & TagMask;
		ptr = set | reinterpret_cast<uintptr_t>(in);
		return *this;
	}

	void clear() noexcept {
		ptr = 0;
	}
	void clear_bits() noexcept {
		ptr = ptr & PointerMask;
	}
	int get_tag() const noexcept {
		return ptr & TagMask;
	}
	void set_tag(int tag) noexcept {
		Expects(tag >= 0 && tag <= static_cast<int>(TagMask));
		ptr = (ptr & PointerMask) | static_cast<uintptr_t>(tag);
	}

	bool test_bit1() const noexcept
		requires(sizeof(void*) >= 2) {
		return ptr & static_cast<uintptr_t>(0b001);
	}
	bool test_bit2() const noexcept
		requires(sizeof(void*) >= 4) {
		return ptr & static_cast<uintptr_t>(0b010);
	}
	bool test_bit3() const noexcept
		requires(sizeof(void*) >= 8) {
		return ptr & static_cast<uintptr_t>(0b100);
	}

	void set_bit1() noexcept
		requires(sizeof(void*) >= 2) {
		ptr |= static_cast<uintptr_t>(0b001);
	}
	void set_bit2() noexcept
		requires(sizeof(void*) >= 4) {
		ptr |= static_cast<uintptr_t>(0b010);
	}
	void set_bit3() noexcept
		requires(sizeof(void*) >= 8) {
		ptr |= static_cast<uintptr_t>(0b100);
	}

	void clear_bit1() noexcept
		requires(sizeof(void*) >= 2) {
		ptr = ptr & ~static_cast<uintptr_t>(0b001);
	}
	void clear_bit2() noexcept
		requires(sizeof(void*) >= 4) {
		ptr = ptr & ~static_cast<uintptr_t>(0b010);
	}
	void clear_bit3() noexcept
		requires(sizeof(void*) >= 8) {
		ptr = ptr & ~static_cast<uintptr_t>(0b100);
	}

	T* pointer() noexcept {
		return reinterpret_cast<T*>(ptr & PointerMask);
	}
	T const* pointer() const noexcept {
		return reinterpret_cast<T*>(ptr & PointerMask);
	}

	T* operator->() noexcept {
		return pointer();
	}
	T const* operator->() const noexcept {
		return pointer();
	}

	operator T*() noexcept {
		return pointer();
	}
	operator T const *() const noexcept {
		return pointer();
	}

private:
	//constexpr static uintptr_t TagMask = 0b111;
	constexpr static uintptr_t TagMask = sizeof(void*) - 1;
	constexpr static uintptr_t PointerMask = ~TagMask;

	uintptr_t ptr;
};

} // namespace ecs::detail

#endif // ECS_DETAIL_TAGGED_POINTER_H
#ifndef ECS_ENTITY_ID
#define ECS_ENTITY_ID

namespace ecs {
namespace detail {
using entity_type = int;
using entity_offset = unsigned int; // must cover the entire entity_type domain
} // namespace detail

// A simple struct that is an entity identifier.
struct entity_id {
	// Uninitialized entity ids are not allowed, because they make no sense
	entity_id() = delete;

	constexpr entity_id(detail::entity_type _id) noexcept : id(_id) {}

	constexpr operator detail::entity_type&() noexcept {
		return id;
	}
	constexpr operator detail::entity_type const &() const noexcept {
		return id;
	}

private:
	detail::entity_type id;
};
} // namespace ecs

#endif // !ECS_ENTITY_ID
#ifndef ECS_ENTITY_ITERATOR
#define ECS_ENTITY_ITERATOR


namespace ecs::detail {

// Iterator support
class entity_iterator {
public:
	// iterator traits
	using difference_type = ptrdiff_t;
	using value_type = entity_type;
	using pointer = const entity_type*;
	using reference = const entity_type&;
	using iterator_category = std::random_access_iterator_tag;

	// entity_iterator() = delete; // no such thing as a 'default' entity
	constexpr entity_iterator() noexcept {};

	constexpr entity_iterator(entity_id ent) noexcept : ent_(ent) {}

	constexpr entity_iterator& operator++() {
		ent_ = step(ent_, 1);
		return *this;
	}

	constexpr entity_iterator operator++(int) {
		entity_iterator const retval = *this;
		++(*this);
		return retval;
	}

	constexpr entity_iterator operator+(difference_type diff) const {
		return entity_iterator{step(ent_, diff)};
	}

	// Operator exclusively for GCC. This operator is called by GCC's parallel implementation
	// for some goddamn reason. When has this ever been a thing?
	constexpr value_type operator[](int index) const {
		return step(ent_, index);
	}

	constexpr value_type operator-(entity_iterator other) const {
		return step(ent_, -other.ent_);
	}

	constexpr bool operator==(entity_iterator other) const {
		return ent_ == other.ent_;
	}

	constexpr bool operator!=(entity_iterator other) const {
		return !(*this == other);
	}

	constexpr entity_id operator*() {
		return {ent_};
	}

protected:
	constexpr entity_type step(entity_type start, difference_type diff) const {
		// ensures the value wraps instead of causing an overflow
		auto const diff_start = static_cast<entity_offset>(start);
		return static_cast<entity_type>(diff_start + diff);
	}

private:
	value_type ent_{0};
};
} // namespace ecs::detail

#endif // !ECS_ENTITY_RANGE
#ifndef ECS_DETAIL_OPTIONS_H
#define ECS_DETAIL_OPTIONS_H


namespace ecs::detail {

//
// Check if type is a group
template <typename T>
struct is_group {
	static constexpr bool value = false;
};
template <typename T>
requires requires {
	T::group_id;
}
struct is_group<T> {
	static constexpr bool value = true;
};

//
// Check if type is an interval
template <typename T>
struct is_interval {
	static constexpr bool value = false;
};
template <typename T>
requires requires {
	T::_ecs_duration;
}
struct is_interval<T> {
	static constexpr bool value = true;
};

//
// Check if type is a parent
template <typename T>
struct is_parent {
	static constexpr bool value = false;
};
template <typename T>
requires requires { typename std::remove_cvref_t<T>::_ecs_parent; }
struct is_parent<T> {
	static constexpr bool value = true;
};



// Contains detectors for the options
namespace detect {
	template <template <typename O> typename Tester, typename ListOptions>
	constexpr int find_tester_index() {
		int found = 0;
		int index = 0;
		for_each_type<ListOptions>([&]<typename OptionFromList>() {
			found = !found && Tester<OptionFromList>::value;
			index += !found;
		});

		if (index == type_list_size<ListOptions>)
			return -1;
		return index;
	}

	template <typename Option, typename ListOptions>
	constexpr int find_type_index() {
		int found = 0;
		int index = 0;
		for_each_type<ListOptions>([&]<typename OptionFromList>() {
			found = !found && std::is_same_v<Option, OptionFromList>;
			index += !found;
		});

		if (index == type_list_size<ListOptions>)
			return -1;
		return index;
	}

	// A detector that applies Tester to each option.
	template <template <typename O> typename Tester, typename ListOptions, typename NotFoundType = void>
	constexpr auto test_option() {
		auto const lambda = []<typename T>() {
			return static_cast<std::remove_cvref_t<T>*>(nullptr);
		};
		using Type = decltype(run_if<Tester, ListOptions>(lambda));

		if constexpr (!std::is_same_v<Type, void>) {
			return Type{};
		} else {
			return static_cast<NotFoundType*>(nullptr);
		}
	}

	template <typename Type>
	struct type_detector {
		template<typename... Types>
		consteval static bool test(type_list<Types...>*) noexcept {
			return (test(static_cast<Types*>(nullptr)) || ...);
		}

		consteval static bool test(Type*) noexcept {
			return true;
		}
		consteval static bool test(...) noexcept {
			return false;
		}
	};

} // namespace detect

// Use a tester to check the options. Takes a tester structure and a type_list of options to test against.
// The tester must have static member 'value' that determines if the option passed to it
// is what it is looking for, see 'is_group' for an example.
// STL testers like 'std::is_execution_policy' can also be used
template <template <typename O> typename Tester, typename TypelistOptions>
using test_option_type = std::remove_pointer_t<decltype(detect::test_option<Tester, TypelistOptions>())>;

// Use a tester to check the options. Results in 'NotFoundType' if the tester
// does not find a viable option.
template <template <typename O> typename Tester, typename TypelistOptions, typename NotFoundType>
using test_option_type_or = std::remove_pointer_t<decltype(detect::test_option<Tester, TypelistOptions, NotFoundType>())>;

// Use a tester to find the index of a type in the tuple. Results in -1 if not found
template <template <typename O> typename Tester, typename TypelistOptions>
static constexpr int test_option_index = detect::find_tester_index<Tester, TypelistOptions>();

template <typename Option, typename ListOptions>
constexpr bool has_option() {
	return detect::type_detector<Option>::test(static_cast<ListOptions*>(nullptr));
}

} // namespace ecs::detail

#endif // !ECS_DETAIL_OPTIONS_H
#ifndef ECS_ENTITY_RANGE
#define ECS_ENTITY_RANGE



namespace ecs {
// Defines a range of entities.
// 'last' is included in the range.
class entity_range final {
	detail::entity_type first_;
	detail::entity_type last_;

public:
	entity_range() = delete; // no such thing as a 'default' range

	constexpr entity_range(detail::entity_type first, detail::entity_type last) : first_(first), last_(last) {
		Expects(first <= last);
	}

	static constexpr entity_range all() {
		return {std::numeric_limits<detail::entity_type>::min(), std::numeric_limits<detail::entity_type>::max()};
	}

	[[nodiscard]] constexpr detail::entity_iterator begin() const {
		return detail::entity_iterator{first_};
	}

	[[nodiscard]] constexpr detail::entity_iterator end() const {
		return detail::entity_iterator{last_} + 1;
	}

	[[nodiscard]] constexpr bool operator==(entity_range const& other) const {
		return equals(other);
	}

	// For sort
	[[nodiscard]] constexpr bool operator<(entity_range const& other) const {
		return /*first_ < other.first() &&*/ last_ < other.first();
	}

	[[nodiscard]] constexpr bool operator<(entity_id const& id) const {
		return last_ < id;
	}

	// Returns the first entity in the range
	[[nodiscard]] constexpr entity_id first() const {
		return entity_id{first_};
	}

	// Returns the last entity in the range
	[[nodiscard]] constexpr entity_id last() const {
		return entity_id{last_};
	}

	// Returns the number of entities in this range
	[[nodiscard]] constexpr ptrdiff_t count() const {
		return static_cast<ptrdiff_t>(last_ - first_ + 1);
	}

	// Returns the number of entities in this range as unsigned
	[[nodiscard]] constexpr size_t ucount() const {
		return static_cast<size_t>(last_ - first_ + 1);
	}

	// Returns true if the ranges are identical
	[[nodiscard]] constexpr bool equals(entity_range const& other) const {
		return first_ == other.first() && last_ == other.last();
	}

	// Returns true if the entity is contained in this range
	[[nodiscard]] constexpr bool contains(entity_id const& ent) const {
		return ent >= first_ && ent <= last_;
	}

	// Returns true if the range is contained in this range
	[[nodiscard]] constexpr bool contains(entity_range const& range) const {
		return range.first() >= first_ && range.last() <= last_;
	}

	// Returns true if the ranges are adjacent to each other
	[[nodiscard]] constexpr bool adjacent(entity_range const& range) const {
		return first_ - 1 == range.last() || last_ + 1 == range.first();
	}

	// Returns the offset of an entity into this range
	// Pre: 'ent' must be in the range
	[[nodiscard]] constexpr detail::entity_offset offset(entity_id const ent) const {
		Expects(contains(ent));
		return static_cast<detail::entity_offset>(ent - first_);
	}

	// Returns the entity id at the specified offset
	// Pre: 'offset' is in the range
	[[nodiscard]] entity_id at(detail::entity_offset const offset) const {
		entity_id const id = static_cast<detail::entity_type>(static_cast<detail::entity_offset>(first()) + offset);
		Expects(id <= last());
		return id;
	}

	// Returns true if the two ranges touches each other
	[[nodiscard]] constexpr bool overlaps(entity_range const& other) const {
		return first_ <= other.last_ && other.first_ <= last_;
	}

	// Removes a range from another range.
	// If the range was split by the remove, it returns two ranges.
	// Pre: 'other' must overlap 'range', but must not be equal to it
	[[nodiscard]] constexpr static std::pair<entity_range, std::optional<entity_range>> remove(entity_range const& range,
																							   entity_range const& other) {
		Expects(!range.equals(other));

		// Remove from the front
		if (other.first() == range.first()) {
			return {entity_range{other.last() + 1, range.last()}, std::nullopt};
		}

		// Remove from the back
		if (other.last() == range.last()) {
			return {entity_range{range.first(), other.first() - 1}, std::nullopt};
		}

		if (range.contains(other)) {
			// Remove from the middle
			return {entity_range{range.first(), other.first() - 1}, entity_range{other.last() + 1, range.last()}};
		} else {
			// Remove overlaps
			Expects(range.overlaps(other));

			if (range.first() < other.first())
				return {entity_range{range.first(), other.first() - 1}, std::nullopt};
			else
				return {entity_range{other.last() + 1, range.last()}, std::nullopt};
		}
	}

	// Combines two ranges into one
	// Pre: r1 and r2 must be adjacent ranges, r1 < r2
	[[nodiscard]] constexpr static entity_range merge(entity_range const& r1, entity_range const& r2) {
		Expects(r1.adjacent(r2));
		if (r1 < r2)
			return entity_range{r1.first(), r2.last()};
		else
			return entity_range{r2.first(), r1.last()};
	}

	// Returns the intersection of two ranges
	// Pre: The ranges must overlap
	[[nodiscard]] constexpr static entity_range intersect(entity_range const& range, entity_range const& other) {
		Expects(range.overlaps(other));

		entity_id const first{std::max(range.first(), other.first())};
		entity_id const last{std::min(range.last(), other.last())};

		return entity_range{first, last};
	}

	// Returns a range that overlaps the two ranges
	[[nodiscard]] constexpr static entity_range overlapping(entity_range const& r1, entity_range const& r2) {
		entity_id const first{std::min(r1.first(), r2.first())};
		entity_id const last{std::max(r1.last(), r2.last())};

		return entity_range{first, last};
	}
};

// The view of a collection of ranges
using entity_range_view = std::span<entity_range const>;

} // namespace ecs

#endif // !ECS_ENTITTY_RANGE
#ifndef ECS_DETAIL_PARENT_H
#define ECS_DETAIL_PARENT_H


namespace ecs::detail {

// The parent type stored internally in component pools
struct parent_id : entity_id {
	constexpr parent_id(detail::entity_type _id) noexcept : entity_id(_id) {}
};

} // namespace ecs::detail

#endif // !ECS_DETAIL_PARENT_H
#ifndef ECS_FLAGS_H
#define ECS_FLAGS_H

// Add flags to a component to change its behaviour and memory usage.
// Example:
// struct my_component {
// 	 ecs_flags(ecs::flag::tag, ecs::flag::transient);
// };
#define ecs_flags(...)                                                                                                                     \
	struct _ecs_flags : __VA_ARGS__ {}

namespace ecs::flag {
// Add this in a component with 'ecs_flags()' to mark it as tag.
// Uses O(1) memory instead of O(n).
// Mutually exclusive with 'share' and 'global'
struct tag {};

// Add this in a component with 'ecs_flags()' to mark it as transient.
// The component will only exist on an entity for one cycle,
// and then be automatically removed.
// Mutually exclusive with 'global'
struct transient {};

// Add this in a component with 'ecs_flags()' to mark it as constant.
// A compile-time error will be raised if a system tries to
// write to the component through a reference.
struct immutable {};

// Add this in a component with 'ecs_flags()' to mark it as global.
// Global components can be referenced from systems without
// having been added to any entities.
// Uses O(1) memory instead of O(n).
// Mutually exclusive with 'tag', 'share', and 'transient'
struct global {};
} // namespace ecs::flag

#endif // !ECS_FLAGS_H
#ifndef ECS_DETAIL_STRIDE_VIEW_H
#define ECS_DETAIL_STRIDE_VIEW_H


namespace ecs::detail {

// 
template <std::size_t Stride, typename T>
class stride_view {
	char const* first{nullptr};
	char const* curr{nullptr};
	char const* last{nullptr};

public:
	stride_view() noexcept = default;
	stride_view(T const* first_, std::size_t count_) noexcept
		: first{reinterpret_cast<char const*>(first_)}
		, curr {reinterpret_cast<char const*>(first_)}
		, last {reinterpret_cast<char const*>(first_) + Stride*count_} {
		Expects(first_ != nullptr);
	}

	T const* current() const noexcept {
		return reinterpret_cast<T const*>(curr);
	}

	bool done() const noexcept {
		return (first==nullptr) || (curr >= last);
	}

	void next() noexcept {
		if (!done())
			curr += Stride;
	}
};

}

#endif //!ECS_DETAIL_STRIDE_VIEW_H
#ifndef ECS_DETAIL_FLAGS_H
#define ECS_DETAIL_FLAGS_H


// Some helpers concepts to detect flags
namespace ecs::detail {

template <typename T>
using flags = typename std::remove_cvref_t<T>::_ecs_flags;

template <typename T>
concept tagged = std::is_base_of_v<ecs::flag::tag, flags<T>>;

template <typename T>
concept transient = std::is_base_of_v<ecs::flag::transient, flags<T>>;

template <typename T>
concept immutable = std::is_base_of_v<ecs::flag::immutable, flags<T>>;

template <typename T>
concept global = std::is_base_of_v<ecs::flag::global, flags<T>>;

template <typename T>
concept local = !global<T>;

template <typename T>
concept persistent = !transient<T>;

template <typename T>
concept unbound = (tagged<T> || global<T>); // component is not bound to a specific entity (ie static)

} // namespace ecs::detail

#endif // !ECS_DETAIL_COMPONENT_FLAGS_H
#ifndef ECS_COMPONENT_POOL_BASE
#define ECS_COMPONENT_POOL_BASE

namespace ecs::detail {
// The baseclass of typed component pools
class component_pool_base {
public:
	component_pool_base() = default;
	component_pool_base(component_pool_base const&) = delete;
	component_pool_base(component_pool_base&&) = delete;
	component_pool_base& operator=(component_pool_base const&) = delete;
	component_pool_base& operator=(component_pool_base&&) = delete;
	virtual ~component_pool_base() = default;

	virtual void process_changes() = 0;
	virtual void clear_flags() = 0;
	virtual void clear() = 0;
};
} // namespace ecs::detail

#endif // !ECS_COMPONENT_POOL_BASE
#ifndef ECS_DETAIL_COMPONENT_POOL_H
#define ECS_DETAIL_COMPONENT_POOL_H





namespace ecs::detail {

#ifdef _MSC_VER
	#define no_unique_address msvc::no_unique_address
#endif

template <typename ForwardIt, typename BinaryPredicate>
ForwardIt std_combine_erase(ForwardIt first, ForwardIt last, BinaryPredicate&& p) noexcept {
	if (first == last)
		return last;

	ForwardIt result = first;
	while (++first != last) {
		auto const pred_res = p(*result, *first);
		if (!pred_res && ++result != first) {
			*result = std::move(*first);
		}
	}
	return ++result;
}

template <typename Cont, typename BinaryPredicate>
void combine_erase(Cont& cont, BinaryPredicate&& p) noexcept {
	auto const end = std_combine_erase(cont.begin(), cont.end(), static_cast<BinaryPredicate&&>(p));
	cont.erase(end, cont.end());
}

template <typename T, typename Alloc = std::allocator<T>>
class component_pool final : public component_pool_base {
private:
	static_assert(!is_parent<T>::value, "can not have pools of any ecs::parent<type>");

	struct chunk {
		chunk() noexcept = default;
		chunk(chunk const&) = delete;
		chunk(chunk&& other) noexcept
			: range{other.range}, active{other.active}, data{other.data} {
			other.data = nullptr;
		}
		chunk& operator=(chunk const&) = delete;
		chunk& operator=(chunk&& other) noexcept {
			range = other.range;
			active = other.active;
			data = other.data;
			other.data = nullptr;
			set_owns_data(other.get_owns_data());
			set_has_split_data(other.get_has_split_data());
			return *this;
		}
		chunk(entity_range range_, entity_range active_, T* data_ = nullptr, bool owns_data_ = false,
						bool has_split_data_ = false) noexcept
			: range(range_), active(active_), data(data_) {
			set_owns_data(owns_data_);
			set_has_split_data(has_split_data_);
		}

		// The full range this chunk covers.
		entity_range range;

		// The partial range of active entities inside this chunk
		entity_range active;

		// The data for the full range of the chunk (range.count())
		// Tagged:
		//   bit1 = owns data
		//   bit2 = has split data
		tagged_pointer<T> data;

		// Signals if this chunk owns this data and should clean it up
		void set_owns_data(bool owns) noexcept {
			if (owns)
				data.set_bit1();
			else
				data.clear_bit1();
		}
		bool get_owns_data() const noexcept {
			return data.test_bit1();
		}

		// Signals if this chunk has been split
		void set_has_split_data(bool split) noexcept {
			if (split)
				data.set_bit2();
			else
				data.clear_bit2();
		}
		bool get_has_split_data() const noexcept {
			return data.test_bit2();
		}
	};
	static_assert(sizeof(chunk) == 24);

	//
	struct entity_empty {
		entity_range rng;
		entity_empty(entity_range r) noexcept : rng{r} {}
	};
	struct entity_data_member : entity_empty {
		T data;
		entity_data_member(entity_range r, T const& t) noexcept : entity_empty{r}, data(t) {}
		entity_data_member(entity_range r, T&& t) noexcept : entity_empty{r}, data(std::forward<T>(t)) {}
	};
	struct entity_span_member : entity_empty {
		std::span<const T> data;
		entity_span_member(entity_range r, std::span<const T> t) noexcept : entity_empty{r}, data(t) {}
	};
	struct entity_gen_member : entity_empty {
		std::function<T(entity_id)> data;
		entity_gen_member(entity_range r, std::function<T(entity_id)>&& t) noexcept
			: entity_empty{r}, data(std::forward<std::function<T(entity_id)>>(t)) {}
	};

	using entity_data = std::conditional_t<unbound<T>, entity_empty, entity_data_member>;
	using entity_span = std::conditional_t<unbound<T>, entity_empty, entity_span_member>;
	using entity_gen = std::conditional_t<unbound<T>, entity_empty, entity_gen_member>;

	using chunk_iter = typename std::vector<chunk>::iterator;
	using chunk_const_iter = typename std::vector<chunk>::const_iterator;

	std::vector<chunk> chunks;

	// Status flags
	bool components_added : 1 = false;
	bool components_removed : 1 = false;
	bool components_modified : 1 = false;

	// Keep track of which components to add/remove each cycle
	[[no_unique_address]] tls::collect<std::vector<entity_data>, component_pool<T>> deferred_adds;
	[[no_unique_address]] tls::collect<std::vector<entity_span>, component_pool<T>> deferred_spans;
	[[no_unique_address]] tls::collect<std::vector<entity_gen>, component_pool<T>> deferred_gen;
	[[no_unique_address]] tls::collect<std::vector<entity_range>, component_pool<T>> deferred_removes;

	[[no_unique_address]] Alloc alloc;

public:
	component_pool() noexcept {
		if constexpr (global<T>) {
			chunks.emplace_back(entity_range::all(), entity_range::all(), nullptr, false, false);
			chunks.front().data = new T[1];
		}
	}
	component_pool(component_pool const&) = delete;
	component_pool(component_pool&&) = delete;
	component_pool& operator=(component_pool const&) = delete;
	component_pool& operator=(component_pool&&) = delete;
	~component_pool() noexcept override {
		if (global<T>) {
			delete [] chunks.front().data.pointer();
		} else {
			free_all_chunks();
		}
	}

	// Add a span of component to a range of entities
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	void add_span(entity_range const range, std::span<const T> span) noexcept requires(!detail::unbound<T>) {
		Expects(range.count() == std::ssize(span));

		// Add the range and function to a temp storage
		deferred_spans.local().emplace_back(range, span);
	}

	// Add a component to a range of entities, initialized by the supplied user function generator
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	template <typename Fn>
	void add_generator(entity_range const range, Fn&& gen) {
		// Add the range and function to a temp storage
		deferred_gen.local().emplace_back(range, std::forward<Fn>(gen));
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	void add(entity_range const range, T&& component) noexcept {
		if constexpr (tagged<T>) {
			deferred_adds.local().emplace_back(range);
		} else {
			deferred_adds.local().emplace_back(range, std::forward<T>(component));
		}
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	void add(entity_range const range, T const& component) noexcept {
		if constexpr (tagged<T>) {
			deferred_adds.local().emplace_back(range);
		} else {
			deferred_adds.local().emplace_back(range, component);
		}
	}

	// Return the shared component
	T& get_shared_component() noexcept requires global<T> {
		return chunks.front().data[0];
	}

	// Remove an entity from the component pool.
	void remove(entity_id const id) noexcept {
		remove({id, id});
	}

	// Remove an entity from the component pool.
	void remove(entity_range const range) noexcept {
		deferred_removes.local().push_back(range);
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	T* find_component_data(entity_id const id) noexcept requires(!global<T>) {
		return const_cast<T*>(std::as_const(*this).find_component_data(id));
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	T const* find_component_data(entity_id const id) const noexcept requires(!global<T>) {
		if (chunks.empty())
			return nullptr;

		thread_local std::size_t tls_cached_chunk_index = 0;
		auto chunk_index = tls_cached_chunk_index;
		if (chunk_index >= std::size(chunks)) [[unlikely]] {
			// Happens when component pools are reset
			chunk_index = 0;
		}

		// Try the cached chunk index first. This will load 2 chunks into a cache line
		if (!chunks[chunk_index].active.contains(id)) {
			// Wasn't found at cached location, so try looking in next chunk.
			// This should result in linear walks being very cheap.
			if ((1+chunk_index) != std::size(chunks) && chunks[1+chunk_index].active.contains(id)) {
				chunk_index += 1;
				tls_cached_chunk_index = chunk_index;
			} else {
				// The id wasn't found in the cached chunks, so do a binary lookup
				auto const range_it = find_in_ordered_active_ranges({id, id});
				if (range_it != chunks.cend() && range_it->active.contains(id)) {
					// cache the index
					chunk_index = static_cast<std::size_t>(ranges_dist(range_it));
					tls_cached_chunk_index = chunk_index;
				} else {
					return nullptr;
				}
			}
		}

		// Do the lookup
		auto const offset = chunks[chunk_index].range.offset(id);
		return &chunks[chunk_index].data[offset];
	}

	// Merge all the components queued for addition to the main storage,
	// and remove components queued for removal
	void process_changes() noexcept override {
		if constexpr (!global<T>) {
			process_remove_components();
			process_add_components();
		}
	}

	// Returns the number of active entities in the pool
	ptrdiff_t num_entities() const noexcept {
		ptrdiff_t count = 0;

		for (chunk const& c : chunks) {
			count += c.active.count();
		}

		return count;
	}

	// Returns the number of active components in the pool
	ptrdiff_t num_components() const noexcept {
		if constexpr (unbound<T>)
			return 1;
		else
			return num_entities();
	}

	// Returns the number of chunks in use
	ptrdiff_t num_chunks() const noexcept {
		return std::ssize(chunks);
	}

	chunk_const_iter get_head_chunk() const noexcept {
		return chunks.begin();
	}

	// Clears the pools state flags
	void clear_flags() noexcept override {
		components_added = false;
		components_removed = false;
		components_modified = false;
	}

	// Returns true if components has been added since last clear_flags() call
	bool has_more_components() const noexcept {
		return components_added;
	}

	// Returns true if components has been removed since last clear_flags() call
	bool has_less_components() const noexcept {
		return components_removed;
	}

	// Returns true if components has been added/removed since last clear_flags() call
	bool has_component_count_changed() const noexcept {
		return components_added || components_removed;
	}

	bool has_components_been_modified() const noexcept {
		return has_component_count_changed() || components_modified;
	}

	// Returns the pools entities
	auto get_entities() const noexcept {
		if (!chunks.empty())
			return stride_view<sizeof(chunk), entity_range const>(&chunks[0].active, chunks.size());
		else
			return stride_view<sizeof(chunk), entity_range const>();
	}

	// Returns true if an entity has a component in this pool
	bool has_entity(entity_id const id) const noexcept {
		return has_entity({id, id});
	}

	// Returns true if an entity range has components in this pool
	bool has_entity(entity_range const& range) const noexcept {
		auto const it = find_in_ordered_active_ranges(range);

		if (it == chunks.end())
			return false;

		return it->active.contains(range);
	}

	// Clear all entities from the pool
	void clear() noexcept override {
		// Remember if components was removed from the pool
		bool const is_removed = (!chunks.empty());

		// Clear all data
		free_all_chunks();
		deferred_adds.reset();
		deferred_spans.reset();
		deferred_gen.reset();
		deferred_removes.reset();
		chunks.clear();
		clear_flags();

		// Save the removal state
		components_removed = is_removed;
	}

	// Flag that components has been modified
	void notify_components_modified() noexcept {
		components_modified = true;
	}

private:
	chunk_iter create_new_chunk(chunk_iter it_loc, entity_range const range, entity_range const active, T* data = nullptr,
								bool owns_data = true, bool split_data = false) noexcept {
		return chunks.emplace(it_loc, range, active, data, owns_data, split_data);
	}

	chunk_iter create_new_chunk(chunk_iter loc, std::forward_iterator auto const& iter) noexcept {
		entity_range const r = iter->rng;
		chunk_iter c = create_new_chunk(loc, r, r);
		if constexpr (!unbound<T>) {
			c->data = alloc.allocate(r.ucount());
			construct_range_in_chunk(c, r, iter->data);
		}

		return c;
	}

	void free_chunk_data(chunk_iter c) noexcept {
		// Check for potential ownership transfer
		if (c->get_owns_data()) {
			auto next = std::next(c);
			if (c->get_has_split_data() && chunks.end() != next) {
				Expects(c->range == next->range);
				// transfer ownership
				next->set_owns_data(true);
			} else {
				if constexpr (!unbound<T>) {
					// Destroy active range
					std::destroy_n(c->data.pointer(), c->active.ucount());

					// Free entire range
					alloc.deallocate(c->data.pointer(), c->range.ucount());

					// Debug
					c->data.clear();
				}
			}
		}
	}

	[[nodiscard]]
	chunk_iter free_chunk(chunk_iter c) noexcept {
		free_chunk_data(c);
		return remove_range_to_chunk(c);
	}

	void free_all_chunks() noexcept {
		if constexpr (!global<T>) {
			auto chunk_it = chunks.begin();
			while (chunk_it != chunks.end()) {
				free_chunk_data(chunk_it);
				std::advance(chunk_it, 1);
			}
			chunks.clear();
			set_data_removed();
		}
	}

	auto find_in_ordered_active_ranges(entity_range const rng) const noexcept {
		return std::ranges::lower_bound(chunks, rng, std::less{}, &chunk::active);
	}

	ptrdiff_t ranges_dist(std::vector<chunk>::const_iterator it) const noexcept {
		return std::distance(chunks.begin(), it);
	}

	// Removes a range and chunk from the map
	[[nodiscard]]
	chunk_iter remove_range_to_chunk(chunk_iter it) noexcept {
		return chunks.erase(it);
	}

	// Flag that components has been added
	void set_data_added() noexcept {
		components_added = true;
	}

	// Flag that components has been removed
	void set_data_removed() noexcept {
		components_removed = true;
	}

	static bool is_equal(T const& lhs, T const& rhs) noexcept requires std::equality_comparable<T> {
		return lhs == rhs;
	}
	static bool is_equal(T const& /*lhs*/, T const& /*rhs*/) noexcept requires tagged<T> {
		// Tags are empty, so always return true
		return true;
	}
	static bool is_equal(T const&, T const&) noexcept {
		// Type can not be compared, so always return false.
		// memcmp is a no-go because it also compares padding in types,
		// and it is not constexpr
		return false;
	}

	template <typename Data>
	void construct_range_in_chunk(chunk_iter c, entity_range range, Data const& comp_data) noexcept requires(!unbound<T>) {
		// Offset into the chunks data
		auto const ent_offset = c->range.offset(range.first());

		entity_id ent = range.first();
		for (size_t i = 0; i < range.ucount(); ++i, ++ent) {
			// Construct from a value or a a span of values
			if constexpr (std::is_same_v<T, Data>) {
				std::construct_at(&c->data[ent_offset + i], comp_data);
			} else if constexpr (std::is_invocable_v<Data, entity_id>) {
				std::construct_at(&c->data[ent_offset + i], comp_data(ent));
			} else {
				std::construct_at(&c->data[ent_offset + i], comp_data[i]);
			}
		}
	}

	void fill_data_in_existing_chunk(chunk_iter& curr, entity_range r) noexcept {
		auto next = std::next(curr);

		// If split chunks are encountered, skip forward to the chunk closest to r
		if (curr->get_has_split_data()) {
			while (chunks.end() != next && next->range.contains(r) && next->active < r) {
				std::advance(curr, 1);
				next = std::next(curr);
			}
		}

		if (curr->active.adjacent(r)) {
			// The two ranges are next to each other, so add the data to existing chunk
			entity_range active_range = entity_range::merge(curr->active, r);
			curr->active = active_range;

			// Check to see if this chunk can be collapsed into 'prev'
			if (chunks.begin() != curr) {
				auto prev = std::next(curr, -1);
				if (prev->active.adjacent(curr->active)) {
					active_range = entity_range::merge(prev->active, curr->active);
					prev = remove_range_to_chunk(prev);
					prev->active = active_range;

					curr = free_chunk(curr);
					next = std::next(curr);
				}
			}

			// Check to see if 'next' can be collapsed into this chunk
			if (chunks.end() != next) {
				if (curr->active.adjacent(next->active)) {
					active_range = entity_range::merge(curr->active, next->active);
					next = free_chunk(next);

					curr->active = active_range;

					// split_data is true if the next chunk is also in the current range
					curr->set_has_split_data((next != chunks.end()) && (curr->range == next->range));
				}
			}
		} else {
			// There is a gap between the two ranges, so split the chunk
			if (r < curr->active) {
				bool const curr_owns_data = curr->get_owns_data();
				curr->set_owns_data(false);
				curr = create_new_chunk(curr, curr->range, r, curr->data, curr_owns_data, true);
			} else {
				curr->set_has_split_data(true);
				curr = create_new_chunk(std::next(curr), curr->range, r, curr->data, false, false);
			}
		}
	}

	// Try to combine two ranges. With data
	static bool combiner_bound(entity_data& a, entity_data const& b) requires(!unbound<T>) {
		if (a.rng.adjacent(b.rng) && is_equal(a.data, b.data)) {
			a.rng = entity_range::merge(a.rng, b.rng);
			return true;
		} else {
			return false;
		}
	}

	// Try to combine two ranges. Without data
	static bool combiner_unbound(entity_data& a, entity_data const& b) requires(unbound<T>) {
		if (a.rng.adjacent(b.rng)) {
			a.rng = entity_range::merge(a.rng, b.rng);
			return true;
		} else {
			return false;
		}
	}

	template <typename U>
	void process_add_components(std::vector<U> const& vec) noexcept {
		if (vec.empty()) {
			return;
		}

		// Do the insertions
		auto iter = vec.begin();
		auto curr = chunks.begin();

		// Fill in values
		while (iter != vec.end()) {
			if (chunks.empty()) {
				curr = create_new_chunk(curr, iter);
			} else {
				entity_range const r = iter->rng;

				// Move current chunk iterator forward
				auto next = std::next(curr);
				while (chunks.end() != next && next->range < r) {
					curr = next;
					std::advance(next, 1);
				}

				if (curr->range.overlaps(r)) {
					// Can not add components more than once to same entity
					Expects(!curr->active.overlaps(r));

					// Incoming range overlaps the current one, so add it into 'curr'
					fill_data_in_existing_chunk(curr, r);
					if constexpr (!unbound<T>) {
						construct_range_in_chunk(curr, r, iter->data);
					}
				} else if (curr->range < r) {
					// Incoming range is larger than the current one, so add it after 'curr'
					curr = create_new_chunk(std::next(curr), iter);
					// std::advance(curr, 1);
				} else if (r < curr->range) {
					// Incoming range is less than the current one, so add it before 'curr' (after 'prev')
					curr = create_new_chunk(curr, iter);
				}
			}

			std::advance(iter, 1);
		}
	}

	// Add new queued entities and components to the main storage.
	void process_add_components() noexcept {
		auto const adder = [this]<typename C>(std::vector<C>& vec) {
			if (vec.empty())
				return;

			// Sort the input(s)
			auto const comparator = [](entity_empty const& l, entity_empty const& r) {
				return l.rng < r.rng;
			};
			std::sort(vec.begin(), vec.end(), comparator);

			// Merge adjacent ranges that has the same data
			if constexpr (std::is_same_v<entity_data*, decltype(vec.data())>) {
				if constexpr (unbound<T>)
					combine_erase(vec, combiner_unbound);
				else
					combine_erase(vec, combiner_bound);
			}

			this->process_add_components(vec);

			// Update the state
			set_data_added();
		};

		deferred_adds.for_each(adder);
		deferred_adds.reset();

		deferred_spans.for_each(adder);
		deferred_spans.reset();

		deferred_gen.for_each(adder);
		deferred_gen.reset();
	}

	// Removes the entities and components
	void process_remove_components() noexcept {
		deferred_removes.for_each([this](std::vector<entity_range>& vec) {
			if (vec.empty())
				return;

			// Sort the ranges to remove
			std::sort(vec.begin(), vec.end());

			// Remove the ranges
			this->process_remove_components(vec);

			// Update the state
			set_data_removed();
		});
		deferred_removes.reset();
	}

	void process_remove_components(std::vector<entity_range>& removes) noexcept {
		chunk_iter it_chunk = chunks.begin();
		auto it_rem = removes.begin();

		while (it_chunk != chunks.end() && it_rem != removes.end()) {
			if (it_chunk->active < *it_rem) {
				std::advance(it_chunk, 1);
			} else if (*it_rem < it_chunk->active) {
				++it_rem;
			} else {
				//if (it_chunk->active == *it_rem) {
				if (it_rem->contains(it_chunk->active)) {
					// Delete the chunk and potentially its data
					it_chunk = free_chunk(it_chunk);
				} else {
					// remove partial range
					auto const [left_range, maybe_split_range] = entity_range::remove(it_chunk->active, *it_rem);

					// Update the active range
					it_chunk->active = left_range;

					// Destroy the removed components
					if constexpr (!unbound<T>) {
						auto const offset = it_chunk->range.offset(it_rem->first());
						std::destroy_n(&it_chunk->data[offset], it_rem->ucount());
					}

					if (maybe_split_range.has_value()) {
						// If two ranges were returned, split this chunk
						it_chunk->set_has_split_data(true);
						it_chunk = create_new_chunk(std::next(it_chunk), it_chunk->range, maybe_split_range.value(), it_chunk->data, false);
					} else {
						std::advance(it_chunk, 1);
					}
				}
			}
		}
	}

	// Removes transient components
	void process_remove_components() noexcept requires transient<T> {
		// All transient components are removed each cycle
		free_all_chunks();
	}
};
} // namespace ecs::detail

#endif // !ECS_DETAIL_COMPONENT_POOL_H
#ifndef ECS_DETAIL_COMPONENT_POOLS_H
#define ECS_DETAIL_COMPONENT_POOLS_H

namespace ecs::detail {

// Forward decls
class component_pool_base;
template <typename, typename> class component_pool;

// 
template <typename ComponentsList>
struct component_pools : type_list_indices<ComponentsList> {
	constexpr component_pools(auto... pools) noexcept : base_pools{pools...} {
		Expects((pools != nullptr) && ...);
	}

	template <typename Component>
	constexpr auto& get() const noexcept {
		constexpr int index = component_pools::index_of(static_cast<Component*>(nullptr));
		return *static_cast<component_pool<Component>*>(base_pools[index]);
	}

	constexpr bool has_component_count_changed() const {
		return any_of_type<ComponentsList>([this]<typename T>() {
			return this->get<T>().has_component_count_changed();
		});
	}

private:
	component_pool_base* base_pools[type_list_size<ComponentsList>];
};

}

#endif //!ECS_DETAIL_COMPONENT_POOLS_H
#ifndef ECS_SYSTEM_DEFS_H_
#define ECS_SYSTEM_DEFS_H_

// Contains definitions that are used by the systems classes

namespace ecs::detail {
template <typename T>
constexpr static bool is_entity = std::is_same_v<std::remove_cvref_t<T>, entity_id>;

// If given a parent, convert to detail::parent_id, otherwise do nothing
template <typename T>
using reduce_parent_t =
	std::conditional_t<std::is_pointer_v<T>, std::conditional_t<is_parent<std::remove_pointer_t<T>>::value, parent_id*, T>,
					   std::conditional_t<is_parent<T>::value, parent_id, T>>;

// Alias for stored pools
template <typename T>
using pool = component_pool<std::remove_pointer_t<std::remove_cvref_t<reduce_parent_t<T>>>>* const;

// Returns true if a type is read-only
template <typename T>
constexpr bool is_read_only() {
	return detail::immutable<T> || detail::tagged<T> || std::is_const_v<std::remove_reference_t<T>>;
}

// Helper to extract the parent types
template <typename T>
struct parent_type_list; // primary template

template <>
struct parent_type_list<void> {
	using type = void;
}; // partial specialization for void

template <template <typename...> typename Parent, typename... ParentComponents> // partial specialization
struct parent_type_list<Parent<ParentComponents...>> {
	static_assert(!(is_parent<ParentComponents>::value || ...), "parents in parents not supported");
	using type = type_list<ParentComponents...>;
};
template <typename T>
using parent_type_list_t = typename parent_type_list<std::remove_cvref_t<T>>::type;

// Helper to extract the parent pool types
template <typename T>
struct parent_pool_detect; // primary template

template <template <typename...> typename Parent, typename... ParentComponents> // partial specialization
struct parent_pool_detect<Parent<ParentComponents...>> {
	static_assert(!(is_parent<ParentComponents>::value || ...), "parents in parents not supported");
	using type = std::tuple<pool<ParentComponents>...>;
};
template <typename T>
using parent_pool_tuple_t = typename parent_pool_detect<T>::type;

// Get a component pool from a component pool tuple.
// Removes cvref and pointer from Component
template <typename Component, typename Pools>
auto& get_pool(Pools const& pools) {
	using T = std::remove_pointer_t<std::remove_cvref_t<reduce_parent_t<Component>>>;
	return pools.template get<T>();
}

// Get a pointer to an entities component data from a component pool tuple.
// If the component type is a pointer, return nullptr
template <typename Component, typename Pools>
Component* get_entity_data([[maybe_unused]] entity_id id, [[maybe_unused]] Pools const& pools) {
	// If the component type is a pointer, return a nullptr
	if constexpr (std::is_pointer_v<Component>) {
		return nullptr;
	} else {
		component_pool<Component>& pool = get_pool<Component>(pools);
		return pool.find_component_data(id);
	}
}

// Get an entities component from a component pool
template <typename Component, typename Pools>
[[nodiscard]] auto get_component(entity_id const entity, Pools const& pools) {
	using T = std::remove_cvref_t<Component>;

	if constexpr (std::is_pointer_v<T>) {
		// Filter: return a nullptr
		static_cast<void>(entity);
		return static_cast<T*>(nullptr);

	} else if constexpr (tagged<T>) {
		// Tag: return a pointer to some dummy storage
		thread_local char dummy_arr[sizeof(T)];
		return reinterpret_cast<T*>(dummy_arr);

	} else if constexpr (global<T>) {
		// Global: return the shared component
		return &get_pool<T>(pools).get_shared_component();

	} else if constexpr (std::is_same_v<reduce_parent_t<T>, parent_id>) {
		return get_pool<parent_id>(pools).find_component_data(entity);

		// Parent component: return the parent with the types filled out
		//parent_id pid = *get_pool<parent_id>(pools).find_component_data(entity);

		// using parent_type = std::remove_cvref_t<Component>;
		// auto const tup_parent_ptrs = for_all_types<parent_type_list_t<parent_type>>(
		//	[&]<typename... ParentTypes>() {
		//		return std::make_tuple(get_entity_data<ParentTypes>(pid, pools)...);
		//	});

		//return parent_type{pid, tup_parent_ptrs};

	} else {
		// Standard: return the component from the pool
		return get_pool<T>(pools).find_component_data(entity);
	}
}

// Extracts a component argument from a tuple
template <typename Component, typename Tuple>
decltype(auto) extract_arg(Tuple& tuple, [[maybe_unused]] ptrdiff_t offset) {
	using T = std::remove_cvref_t<Component>;

	if constexpr (std::is_pointer_v<T>) {
		return nullptr;
	} else if constexpr (detail::unbound<T>) {
		T* ptr = std::get<T*>(tuple);
		return *ptr;
	} else if constexpr (detail::is_parent<T>::value) {
		return std::get<T>(tuple);
	} else {
		T* ptr = std::get<T*>(tuple);
		return *(ptr + offset);
	}
}

// Extracts a component argument from a pointer+offset
template <typename Component>
decltype(auto) extract_arg_lambda(auto& cmp, [[maybe_unused]] ptrdiff_t offset, [[maybe_unused]] auto pools = std::ptrdiff_t{0}) {
	using T = std::remove_cvref_t<Component>;

	if constexpr (std::is_pointer_v<T>) {
		return static_cast<T>(nullptr);
	} else if constexpr (detail::unbound<T>) {
		T* ptr = cmp;
		return *ptr;
	} else if constexpr (detail::is_parent<T>::value) {
		parent_id const pid = *(cmp + offset);

		// TODO store this in seperate container in system_hierarchy? might not be
		//      needed after O(1) pool lookup implementation
		using parent_type = std::remove_cvref_t<Component>;
		return for_all_types<parent_type_list_t<parent_type>>([&]<typename... ParentTypes>() {
			return parent_type{pid, get_entity_data<ParentTypes>(pid, pools)...};
		});
	} else {
		T* ptr = cmp;
		return *(ptr + offset);
	}
}

// The type of a single component argument
template <typename Component>
using component_argument = std::conditional_t<is_parent<std::remove_cvref_t<Component>>::value,
											  std::remove_cvref_t<reduce_parent_t<Component>>*,	// parent components are stored as copies
											  std::remove_cvref_t<Component>*>; // rest are pointers
} // namespace ecs::detail

#endif // !ECS_SYSTEM_DEFS_H_
#ifndef ECS_PARENT_H_
#define ECS_PARENT_H_


// forward decls
namespace ecs::detail {
	template <typename Pools> struct pool_entity_walker;
	template <typename Pools> struct pool_range_walker;

	template<std::size_t Size>
	struct void_ptr_storage {
		void* te_ptrs[Size];
	};
	struct empty_storage {
	};
}

namespace ecs {
// Special component that allows parent/child relationships
template <typename... ParentTypes>
struct parent : entity_id,
				private std::conditional_t<(sizeof...(ParentTypes) > 0), detail::void_ptr_storage<sizeof...(ParentTypes)>, detail::empty_storage> {

	explicit parent(entity_id id) : entity_id(id) {}

	parent(parent const&) = default;
	parent& operator=(parent const&) = default;

	entity_id id() const {
		return static_cast<entity_id>(*this);
	}

	template <typename T>
	[[nodiscard]] T& get() const {
		static_assert((std::is_same_v<T, ParentTypes> || ...), "T is not specified in the parent component");
		return *static_cast<T*>(this->te_ptrs[detail::index_of<T, parent_type_list>()]);
	}

	// used internally by detectors
	struct _ecs_parent {};

private:
	using parent_type_list = detail::type_list<ParentTypes...>;

	template <typename Component>
	friend decltype(auto) detail::extract_arg_lambda(auto& cmp, ptrdiff_t offset, auto pools);

	template <typename Pools> friend struct detail::pool_entity_walker;
	template <typename Pools> friend struct detail::pool_range_walker;

	parent(entity_id id, ParentTypes*... pt)
		requires(sizeof...(ParentTypes) > 0)
		: entity_id(id) {
		((this->te_ptrs[detail::index_of<ParentTypes, parent_type_list>()] = static_cast<void*>(pt)), ...);
	}
};
} // namespace ecs

#endif // !ECS_PARENT_H_
#ifndef ECS_OPTIONS_H
#define ECS_OPTIONS_H

namespace ecs::opts {
template <int I>
struct group {
	static constexpr int group_id = I;
};

template <int Milliseconds, int Microseconds = 0>
struct interval {
	static_assert(Milliseconds >= 0, "invalid time values specified");
	static_assert(Microseconds >= 0 && Microseconds < 1000, "invalid time values specified");

	static constexpr double _ecs_duration = (1.0 * Milliseconds) + (Microseconds / 1000.0);
	static constexpr int _ecs_duration_ms = Milliseconds;
	static constexpr int _ecs_duration_us = Microseconds;
};

struct manual_update {};

struct not_parallel {};
// struct not_concurrent {};

} // namespace ecs::opts

#endif // !ECS_OPTIONS_H
#ifndef ECS_FREQLIMIT_H
#define ECS_FREQLIMIT_H


namespace ecs::detail {

template <int milliseconds, int microseconds>
struct interval_limiter {
	bool can_run() {
		using namespace std::chrono_literals;
		constexpr std::chrono::nanoseconds interval_size = 1ms * milliseconds + 1us * microseconds;

		auto const now = std::chrono::high_resolution_clock::now();
		auto const diff = now - time;
		if (diff >= interval_size) {
			time = now;
			return true;
		} else {
			return false;
		}
	}

private:
	std::chrono::high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now();
};

struct no_interval_limiter {
	constexpr bool can_run() {
		return true;
	}
};

} // namespace ecs::detail

#endif // !ECS_FREQLIMIT_H
#ifndef POOL_ENTITY_WALKER_H_
#define POOL_ENTITY_WALKER_H_


namespace ecs::detail {

// The type of a single component argument
template <typename Component>
using walker_argument = reduce_parent_t<std::remove_cvref_t<Component>>*;

template <typename T>
struct pool_type_detect; // primary template

template <template <typename> typename Pool, typename Type>
struct pool_type_detect<Pool<Type>> {
	using type = Type;
};
template <template <typename> typename Pool, typename Type>
struct pool_type_detect<Pool<Type>*> {
	using type = Type;
};

template <typename T>
struct tuple_pool_type_detect; // primary template

template <template <typename...> typename Tuple, typename... PoolTypes> // partial specialization
struct tuple_pool_type_detect<const Tuple<PoolTypes* const...>> {
	using type = std::tuple<typename pool_type_detect<PoolTypes>::type*...>;
};

template <typename T>
using tuple_pool_type_detect_t = typename tuple_pool_type_detect<T>::type;

// Linearly walks one-or-more component pools
// TODO why is this not called an iterator?
template <typename Pools>
struct pool_entity_walker {
	void reset(Pools* _pools, entity_range_view view) {
		pools = _pools;
		ranges = view;
		ranges_it = ranges.begin();
		offset = 0;

		//update_pool_offsets();
	}

	bool done() const {
		return ranges_it == ranges.end();
	}

	void next_range() {
		++ranges_it;
		offset = 0;

		//if (!done())
		//	update_pool_offsets();
	}

	void next() {
		if (offset == static_cast<entity_type>(ranges_it->count()) - 1) {
			next_range();
		} else {
			++offset;
		}
	}

	// Get the current range
	entity_range get_range() const {
		Expects(!done());
		return *ranges_it;
	}

	// Get the current entity
	entity_id get_entity() const {
		Expects(!done());
		return ranges_it->first() + offset;
	}

	// Get an entities component from a component pool
	template <typename Component>
	[[nodiscard]] auto get() const {
		return get_component<Component>(get_entity(), *pools);
	}

private:
	// The ranges to iterate over
	entity_range_view ranges;

	// Iterator over the current range
	entity_range_view::iterator ranges_it;

	// Pointers to the start of each pools data
	//tuple_pool_type_detect_t<Pools> pointers;

	// Entity id and pool-pointers offset
	entity_type offset;

	// The tuple of pools in use
	Pools* pools;
};

} // namespace ecs::detail

#endif // !POOL_ENTITY_WALKER_H_
#ifndef POOL_RANGE_WALKER_H_
#define POOL_RANGE_WALKER_H_


namespace ecs::detail {

// Linearly walks one-or-more component pools
template <typename Pools>
struct pool_range_walker {
	pool_range_walker(Pools const _pools) : pools(_pools) {}

	void reset(entity_range_view view) {
		ranges.assign(view.begin(), view.end());
		it = ranges.begin();
	}

	bool done() const {
		return it == ranges.end();
	}

	void next() {
		++it;
	}

	// Get the current range
	entity_range get_range() const {
		return *it;
	}

	// Get an entities component from a component pool
	template <typename Component>
	[[nodiscard]] auto get() const {
		return get_component<Component>(get_range().first(), pools);
	}

private:
	std::vector<entity_range> ranges;
	std::vector<entity_range>::iterator it;
	Pools const pools;
};

} // namespace ecs::detail

#endif // !POOL_RANGE_WALKER_H_
#ifndef _ENTITY_OFFSET_H
#define _ENTITY_OFFSET_H


namespace ecs::detail {

class entity_offset_conv {
	entity_range_view ranges;
	std::vector<int> range_offsets;

public:
	entity_offset_conv(entity_range_view _ranges) noexcept : ranges(_ranges) {
		range_offsets.resize(ranges.size());
		std::exclusive_scan(ranges.begin(), ranges.end(), range_offsets.begin(), int{0},
							[](int val, entity_range r) { return val + static_cast<int>(r.count()); });
	}

	bool contains(entity_id ent) const noexcept {
		auto const it = std::lower_bound(ranges.begin(), ranges.end(), ent);
		if (it == ranges.end() || !it->contains(ent))
			return false;
		else
			return true;
	}

	int to_offset(entity_id ent) const noexcept {
		auto const it = std::lower_bound(ranges.begin(), ranges.end(), ent);
		Expects(it != ranges.end() && it->contains(ent)); // Expects the entity to be in the ranges

		auto const offset = static_cast<std::size_t>(std::distance(ranges.begin(), it));
		return range_offsets[offset] + (ent - it->first());
	}

	entity_id from_offset(int offset) const noexcept {
		auto const it = std::upper_bound(range_offsets.begin(), range_offsets.end(), offset);
		auto const dist = std::distance(range_offsets.begin(), it);
		auto const dist_prev = static_cast<std::size_t>(std::max(ptrdiff_t{0}, dist - 1));
		return static_cast<entity_id>(ranges[dist_prev].first() + offset - range_offsets[dist_prev]);
	}
};

} // namespace ecs::detail

#endif // !_ENTITY_OFFSET_H
#ifndef ECS_VERIFICATION_H
#define ECS_VERIFICATION_H



namespace ecs::detail {

// Given a type T, if it is callable with an entity argument,
// resolve to the return type of the callable. Otherwise assume the type T.
template <typename T>
using get_type_t = std::conditional_t<std::invocable<T, entity_type>,
	std::invoke_result<T, entity_type>,
	std::type_identity<T>>;


// Returns true if all types passed are unique
template <typename First, typename... T>
consteval bool is_unique_types() {
	if constexpr ((std::is_same_v<First, T> || ...))
		return false;
	else {
		if constexpr (sizeof...(T) == 0)
			return true;
		else
			return is_unique_types<T...>();
	}
}


// Find the types a sorting predicate takes
template <typename R, typename T>
consteval std::remove_cvref_t<T> get_sorter_type(R (*)(T, T)) { return T{}; }			// Standard function

template <typename R, typename C, typename T>
consteval std::remove_cvref_t<T> get_sorter_type(R (C::*)(T, T) const) {return T{}; }	// const member function
template <typename R, typename C, typename T>
consteval std::remove_cvref_t<T> get_sorter_type(R (C::*)(T, T) ) {return T{}; }			// mutable member function


template <typename Pred>
consteval auto get_sorter_type() {
	// Verify predicate
	static_assert(
		requires {
			{ Pred{}({}, {}) } -> std::same_as<bool>;
		},
		"predicates must take two arguments and return a bool");

	if constexpr (requires { &Pred::operator(); }) {
		return get_sorter_type(&Pred::operator());
	} else {
		return get_sorter_type(Pred{});
	}
}

template <typename Pred>
using sorter_predicate_type_t = decltype(get_sorter_type<Pred>());


// Implement the requirements for ecs::parent components
template <typename C>
consteval void verify_parent_component() {
	if constexpr (detail::is_parent<C>::value) {
		using parent_subtypes = parent_type_list_t<C>;
		size_t const total_subtypes = type_list_size<parent_subtypes>;

		if constexpr (total_subtypes > 0) {
			// Count all the filters in the parent type
			size_t const num_subtype_filters =
				for_all_types<parent_subtypes>([]<typename... Types>() { return (std::is_pointer_v<Types> + ...); });

			// Count all the types minus filters in the parent type
			size_t const num_parent_subtypes = total_subtypes - num_subtype_filters;

			// If there is one-or-more sub-components,
			// then the parent must be passed as a const reference or value
			if constexpr (num_parent_subtypes > 0) {
				if constexpr (std::is_reference_v<C>) {
					static_assert(std::is_const_v<std::remove_reference_t<C>>, "parent with non-filter sub-components must be passed as a value or const&");
				}
			}
		}
	}
}

// Implement the requirements for tagged components
template <typename C>
consteval void verify_tagged_component() {
	if constexpr (detail::tagged<C>)
		static_assert(!std::is_reference_v<C> && (sizeof(C) == 1), "components flagged as 'tag' must not be references");
}

// Implement the requirements for global components
template <typename C>
consteval void verify_global_component() {
	if constexpr (detail::global<C>)
		static_assert(!detail::tagged<C> && !detail::transient<C>, "components flagged as 'global' must not be 'tag's or 'transient'");
}

// Implement the requirements for immutable components
template <typename C>
consteval void verify_immutable_component() {
	if constexpr (detail::immutable<C>)
		static_assert(std::is_const_v<std::remove_reference_t<C>>, "components flagged as 'immutable' must also be const");
}

template <typename R, typename FirstArg, typename... Args>
consteval void system_verifier() {
	static_assert(std::is_same_v<R, void>, "systems can not have returnvalues");

	static_assert(is_unique_types<FirstArg, Args...>(), "component parameter types can only be specified once");

	if constexpr (is_entity<FirstArg>) {
		static_assert(sizeof...(Args) > 0, "systems must take at least one component argument");

		// Make sure the first entity is not passed as a reference
		static_assert(!std::is_reference_v<FirstArg>, "ecs::entity_id must not be passed as a reference");
	} else {
		verify_immutable_component<FirstArg>();
		verify_global_component<FirstArg>();
		verify_tagged_component<FirstArg>();
		verify_parent_component<FirstArg>();
	}

	(verify_immutable_component<Args>(), ...);
	(verify_global_component<Args>(), ...);
	(verify_tagged_component<Args>(), ...);
	(verify_parent_component<Args>(), ...);
}

// A small bridge to allow the Lambda to activate the system verifier
template <typename R, typename C, typename FirstArg, typename... Args>
consteval void system_to_lambda_bridge(R (C::*)(FirstArg, Args...)) { system_verifier<R, FirstArg, Args...>(); }
template <typename R, typename C, typename FirstArg, typename... Args>
consteval void system_to_lambda_bridge(R (C::*)(FirstArg, Args...) const) { system_verifier<R, FirstArg, Args...>(); }
template <typename R, typename C, typename FirstArg, typename... Args>
consteval void system_to_lambda_bridge(R (C::*)(FirstArg, Args...) noexcept) { system_verifier<R, FirstArg, Args...>(); }
template <typename R, typename C, typename FirstArg, typename... Args>
consteval void system_to_lambda_bridge(R (C::*)(FirstArg, Args...) const noexcept) { system_verifier<R, FirstArg, Args...>(); }

// A small bridge to allow the function to activate the system verifier
template <typename R, typename FirstArg, typename... Args>
consteval void system_to_func_bridge(R (*)(FirstArg, Args...)) { system_verifier<R, FirstArg, Args...>(); }
template <typename R, typename FirstArg, typename... Args>
consteval void system_to_func_bridge(R (*)(FirstArg, Args...) noexcept) { system_verifier<R, FirstArg, Args...>(); }

template <typename T>
concept type_is_lambda = requires {
	&T::operator();
};

template <typename T>
concept type_is_function = requires(T t) {
	system_to_func_bridge(t);
};

template <typename OptionsTypeList, typename SystemFunc, typename SortFunc>
consteval void make_system_parameter_verifier() {
	bool constexpr is_lambda = type_is_lambda<SystemFunc>;
	bool constexpr is_func = type_is_function<SystemFunc>;

	static_assert(is_lambda || is_func, "Systems can only be created from lambdas or free-standing functions");

	// verify the system function
	if constexpr (is_lambda) {
		system_to_lambda_bridge(&SystemFunc::operator());
	} else if constexpr (is_func) {
		system_to_func_bridge(SystemFunc{});
	}

	// verify the sort function
	if constexpr (!std::is_same_v<std::nullptr_t, SortFunc>) {
		bool constexpr is_sort_lambda = type_is_lambda<SortFunc>;
		bool constexpr is_sort_func = type_is_function<SortFunc>;

		static_assert(is_sort_lambda || is_sort_func, "invalid sorting function");

		using sort_types = sorter_predicate_type_t<SortFunc>;
		static_assert(std::predicate<SortFunc, sort_types, sort_types>, "Sorting function is not a predicate");
	}
}

} // namespace ecs::detail

#endif // !ECS_VERIFICATION_H
#ifndef ECS_DETAIL_ENTITY_RANGE
#define ECS_DETAIL_ENTITY_RANGE


// Find the intersectsions between two sets of ranges
namespace ecs::detail {
template <typename Iter>
struct iter_pair {
	Iter curr;
	Iter end;
};

template <typename Iter1, typename Iter2>
std::vector<entity_range> intersect_ranges_iter(iter_pair<Iter1> it_a, iter_pair<Iter2> it_b) {
	std::vector<entity_range> result;

	while (it_a.curr != it_a.end && it_b.curr != it_b.end) {
		if (it_a.curr->overlaps(*it_b.curr)) {
			result.push_back(entity_range::intersect(*it_a.curr, *it_b.curr));
		}

		if (it_a.curr->last() < it_b.curr->last()) { // range a is inside range b, move to
													 // the next range in a
			++it_a.curr;
		} else if (it_b.curr->last() < it_a.curr->last()) { // range b is inside range a,
															// move to the next range in b
			++it_b.curr;
		} else { // ranges are equal, move to next ones
			++it_a.curr;
			++it_b.curr;
		}
	}

	return result;
}

// Find the intersectsions between two sets of ranges
/* inline std::vector<entity_range> intersect_ranges(entity_range_view view_a, entity_range_view view_b) {
	std::vector<entity_range> result;

	if (view_a.empty() || view_b.empty()) {
		return result;
	}

	auto it_a = view_a.begin();
	auto it_b = view_b.begin();

	while (it_a != view_a.end() && it_b != view_b.end()) {
		if (it_a->overlaps(*it_b)) {
			result.push_back(entity_range::intersect(*it_a, *it_b));
		}

		if (it_a->last() < it_b->last()) { // range a is inside range b, move to
										   // the next range in a
			++it_a;
		} else if (it_b->last() < it_a->last()) { // range b is inside range a,
												  // move to the next range in b
			++it_b;
		} else { // ranges are equal, move to next ones
			++it_a;
			++it_b;
		}
	}

	return result;
}*/

// Merges a range into the last range in the vector, or adds a new range
inline void merge_or_add(std::vector<entity_range>& v, entity_range r) {
	if (!v.empty() && v.back().adjacent(r))
		v.back() = entity_range::merge(v.back(), r);
	else
		v.push_back(r);
}

// Find the difference between two sets of ranges.
// Removes ranges in b from a.
inline std::vector<entity_range> difference_ranges(entity_range_view view_a, entity_range_view view_b) {
	if (view_a.empty())
		return {};
	if (view_b.empty())
		return {view_a.begin(), view_a.end()};

	std::vector<entity_range> result;
	auto it_a = view_a.begin();
	auto it_b = view_b.begin();

	auto range_a = *it_a;
	while (it_a != view_a.end()) {
		if (it_b == view_b.end()) {
			result.push_back(range_a);
			if (++it_a != view_a.end())
				range_a = *it_a;
		} else if (it_b->contains(range_a)) {
			// Range 'a' is contained entirely in range 'b',
			// which means that 'a' will not be added.
			if (++it_a != view_a.end())
				range_a = *it_a;
		} else if (range_a < *it_b) {
			// The whole 'a' range is before 'b', so add range 'a'
			result.push_back(range_a);

			if (++it_a != view_a.end())
				range_a = *it_a;
		} else if (*it_b < range_a) {
			// The whole 'b' range is before 'a', so move ahead
			++it_b;
		} else {
			// The two ranges overlap

			auto const res = entity_range::remove(range_a, *it_b);

			if (res.second) {
				// Range 'a' was split in two by range 'b'. Add the first range and update
				// range 'a' with the second range
				result.push_back(res.first);
				range_a = *res.second;

				++it_b;
			} else {
				// Range 'b' removes some of range 'a'

				if (range_a.first() >= it_b->first()) {
					// The result is an endpiece, so update the current range.
					// The next 'b' might remove more from the current 'a'
					range_a = res.first;

					++it_b;
				} else {
					// Add the range
					result.push_back(res.first);

					if (++it_a != view_a.end())
						range_a = *it_a;
				}
			}
		}
	}

	return result;
}
} // namespace ecs::detail

#endif // !ECS_DETAIL_ENTITY_RANGE
#ifndef ECS_FIND_ENTITY_POOL_INTERSECTIONS_H
#define ECS_FIND_ENTITY_POOL_INTERSECTIONS_H


namespace ecs::detail {

template <typename Component, typename TuplePools>
void pool_intersect(std::vector<entity_range>& ranges, TuplePools const& pools) {
	using T = std::remove_cvref_t<Component>;
	using iter1 = typename std::vector<entity_range>::iterator;
	using iter2 = typename entity_range_view::iterator;

	// Skip globals and parents
	if constexpr (detail::global<T>) {
		// do nothing
	} else if constexpr (detail::is_parent<T>::value) {
		auto const ents = get_pool<parent_id>(pools).get_entities();
		ranges = intersect_ranges_iter(iter_pair<iter1>{ranges.begin(), ranges.end()}, iter_pair<iter2>{ents.begin(), ents.end()});
	} else if constexpr (std::is_pointer_v<T>) {
		// do nothing
	} else {
		// ranges = intersect_ranges(ranges, get_pool<T>(pools).get_entities());
		auto const ents = get_pool<T>(pools).get_entities();
		ranges = intersect_ranges_iter(iter_pair<iter1>{ranges.begin(), ranges.end()}, iter_pair<iter2>{ents.begin(), ents.end()});
	}
}

template <typename Component, typename TuplePools>
void pool_difference(std::vector<entity_range>& ranges, TuplePools const& pools) {
	using T = std::remove_cvref_t<Component>;

	if constexpr (std::is_pointer_v<T>) {
		using NoPtr = std::remove_pointer_t<T>;

		if constexpr (detail::is_parent<NoPtr>::value) {
			ranges = difference_ranges(ranges, get_pool<parent_id>(pools).get_entities());
		} else {
			ranges = difference_ranges(ranges, get_pool<NoPtr>(pools).get_entities());
		}
	}
}

// Find the intersection of the sets of entities in the specified pools
template <typename FirstComponent, typename... Components, typename TuplePools>
std::vector<entity_range> find_entity_pool_intersections(TuplePools const& pools) {
	std::vector<entity_range> ranges{entity_range::all()};

	if constexpr (std::is_same_v<entity_id, FirstComponent>) {
		(pool_intersect<Components, TuplePools>(ranges, pools), ...);
		(pool_difference<Components, TuplePools>(ranges, pools), ...);
	} else {
		pool_intersect<FirstComponent, TuplePools>(ranges, pools);
		(pool_intersect<Components, TuplePools>(ranges, pools), ...);

		pool_difference<FirstComponent, TuplePools>(ranges, pools);
		(pool_difference<Components, TuplePools>(ranges, pools), ...);
	}

	return ranges;
}

template <typename ComponentList, typename Pools>
auto get_pool_iterators([[maybe_unused]] Pools pools) {
	if constexpr (type_list_size<ComponentList> > 0) {
		return for_all_types<ComponentList>([&]<typename... Components>() {
			return std::to_array({get_pool<Components>(pools).get_entities()...});
		});
	} else {
		return std::array<stride_view<0,char const>, 0>{};
	}
}


// Find the intersection of the sets of entities in the specified pools
template <typename ComponentList, typename Pools, typename F>
void find_entity_pool_intersections_cb(Pools pools, F callback) {
	static_assert(0 < type_list_size<ComponentList>, "Empty component list supplied");

	// Split the type_list into filters and non-filters (regular components)
	using SplitPairList = split_types_if<ComponentList, std::is_pointer>;
	auto iter_filters = get_pool_iterators<typename SplitPairList::first>(pools);
	auto iter_components = get_pool_iterators<typename SplitPairList::second>(pools);

	// Sort the filters
	std::sort(iter_filters.begin(), iter_filters.end(), [](auto const& a, auto const& b) {
		return *a.current() < *b.current();
	});

	// helper lambda to test if an iterator has reached its end
	auto const done = [](auto it) {
		return it.done();
	};

	while (!std::any_of(iter_components.begin(), iter_components.end(), done)) {
		// Get the starting range to test other ranges against
		entity_range curr_range = *iter_components[0].current();

		// Find all intersections
		if constexpr (type_list_size<typename SplitPairList::second> == 1) {
			iter_components[0].next();
		} else {
			bool intersection_found = false;
			for (size_t i = 1; i < iter_components.size(); ++i) {
				auto& it_a = iter_components[i - 1];
				auto& it_b = iter_components[i];

				if (curr_range.overlaps(*it_b.current())) {
					curr_range = entity_range::intersect(curr_range, *it_b.current());
					intersection_found = true;
				}

				if (it_a.current()->last() < it_b.current()->last()) {
					// range a is inside range b, move to
					// the next range in a
					it_a.next();
					if (it_a.done())
						break;

				} else if (it_b.current()->last() < it_a.current()->last()) {
					// range b is inside range a,
					// move to the next range in b
					it_b.next();
				} else {
					// ranges are equal, move to next ones
					it_a.next();
					if (it_a.done())
						break;

					it_b.next();
				}
			}

			if (!intersection_found)
				continue;
		}

		// Filter the range, if needed
		if constexpr (type_list_size<typename SplitPairList::first> > 0) {
			bool completely_filtered = false;
			for (auto& it : iter_filters) {
				while(!done(it)) {
					if (it.current()->contains(curr_range)) {
						// 'curr_range' is contained entirely in filter range,
						// which means that it will not be sent to the callback
						completely_filtered = true;
						break;
					} else if (curr_range < *it.current()) {
						// The whole 'curr_range' is before the filter, so don't touch it
						break;
					} else if (*it.current() < curr_range) {
						// The filter precedes the range, so advance it and restart
						it.next();
						continue;
					} else {
						// The two ranges overlap
						auto const res = entity_range::remove(curr_range, *it.current());

						if (res.second) {
							// 'curr_range' was split in two by the filter.
							// Send the first range and update
							// 'curr_range' to be the second range
							callback(res.first);
							curr_range = *res.second;

							it.next();
						} else {
							// The result is an endpiece, so update the current range.
							// The next filter might remove more from 'curr_range'
							curr_range = res.first;

							if (curr_range.first() >= it.current()->first()) {
								it.next();
							}
						}
					}
				}
			}

			if (!completely_filtered)
				callback(curr_range);
		} else {
			// No filters on this range, so send
			callback(curr_range);
		}
	}
}

} // namespace ecs::detail

#endif // !ECS_FIND_ENTITY_POOL_INTERSECTIONS_H
#ifndef ECS_SYSTEM_BASE
#define ECS_SYSTEM_BASE


namespace ecs::detail {
class context;

class system_base {
public:
	system_base() = default;
	virtual ~system_base() = default;
	system_base(system_base const&) = delete;
	system_base(system_base&&) = default;
	system_base& operator=(system_base const&) = delete;
	system_base& operator=(system_base&&) = default;

	// Run this system on all of its associated components
	virtual void run() = 0;

	// Enables this system for updates and runs
	void enable() {
		set_enable(true);
	}

	// Prevent this system from being updated or run
	void disable() {
		set_enable(false);
	}

	// Sets wheter the system is enabled or disabled
	void set_enable(bool is_enabled) {
		enabled = is_enabled;
		if (is_enabled) {
			process_changes(true);
		}
	}

	// Returns true if this system is enabled
	[[nodiscard]] bool is_enabled() const {
		return enabled;
	}

	// Returns the group this system belongs to
	[[nodiscard]] virtual int get_group() const noexcept = 0;

	// Get the hashes of types used by the system with const/reference qualifiers removed
	[[nodiscard]] virtual std::span<detail::type_hash const> get_type_hashes() const noexcept = 0;

	// Returns true if this system uses the type
	[[nodiscard]] virtual bool has_component(detail::type_hash hash) const noexcept = 0;

	// Returns true if this system has a dependency on another system
	[[nodiscard]] virtual bool depends_on(system_base const*) const noexcept = 0;

	// Returns true if this system writes data to a specific component
	[[nodiscard]] virtual bool writes_to_component(detail::type_hash hash) const noexcept = 0;

private:
	// Only allow the context class to call 'process_changes'
	friend class detail::context;

	// Process changes to component layouts
	virtual void process_changes(bool force_rebuild = false) = 0;

	// Whether this system is enabled or disabled. Disabled systems are neither updated nor run.
	bool enabled = true;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_BASE
#ifndef ECS_SYSTEM
#define ECS_SYSTEM



namespace ecs::detail {

// The implementation of a system specialized on its components
template <typename Options, typename UpdateFn, typename Pools, bool FirstIsEntity, typename ComponentsList>
class system : public system_base {
	virtual void do_run() = 0;
	virtual void do_build() = 0;

public:
	system(UpdateFn func, Pools in_pools) : update_func{func}, pools{in_pools} {
	}

	void run() override {
		if (!is_enabled()) {
			return;
		}

		if (!interval_checker.can_run()) {
			return;
		}

		do_run();

		// Notify pools if data was written to them
		for_each_type<ComponentsList>([this]<typename T>() {
			this->notify_pool_modifed<T>();
		});
	}

	template <typename T>
	void notify_pool_modifed() {
		if constexpr (detail::is_parent<T>::value && !is_read_only<T>()) { // writeable parent
			// Recurse into the parent types
			for_each_type<parent_type_list_t<T>>([this]<typename... ParentTypes>() {
				(this->notify_pool_modifed<ParentTypes>(), ...);
			});
		} else if constexpr (std::is_reference_v<T> && !is_read_only<T>() && !std::is_pointer_v<T>) {
			get_pool<T>(pools).notify_components_modified();
		}
	}

	constexpr int get_group() const noexcept override {
		using group = test_option_type_or<is_group, Options, opts::group<0>>;
		return group::group_id;
	}

	constexpr std::span<detail::type_hash const> get_type_hashes() const noexcept override {
		return type_hashes;
	}

	constexpr bool has_component(detail::type_hash hash) const noexcept override {
		auto const check_hash = [hash]<typename T>() {
			return get_type_hash<T>() == hash;
		};

		if (any_of_type<stripped_component_list>(check_hash))
			return true;

		if constexpr (has_parent_types) {
			return any_of_type<parent_component_list>(check_hash);
		} else {
			return false;
		}
	}

	constexpr bool depends_on(system_base const* other) const noexcept override {
		return any_of_type<stripped_component_list>([this, other]<typename T>() {
			constexpr auto hash = get_type_hash<T>();

			// If the other system doesn't touch the same component,
			// then there can be no dependecy
			if (!other->has_component(hash))
				return false;

			bool const other_writes = other->writes_to_component(hash);
			if (other_writes) {
				// The other system writes to the component,
				// so there is a strong dependency here.
				// Order is preserved.
				return true;
			} else { // 'other' reads component
				bool const this_writes = writes_to_component(hash);
				if (this_writes) {
					// This system writes to the component,
					// so there is a strong dependency here.
					// Order is preserved.
					return true;
				} else {
					// These systems have a weak read/read dependency
					// and can be scheduled concurrently
					// Order does not need to be preserved.
					return false;
				}
			}
		});
	}

	constexpr bool writes_to_component(detail::type_hash hash) const noexcept override {
		auto const check_writes = [hash]<typename T>() {
			return get_type_hash<std::remove_cvref_t<T>>() == hash && !is_read_only<T>();
		};

		if (any_of_type<ComponentsList>(check_writes))
			return true;

		if constexpr (has_parent_types) {
			return any_of_type<parent_component_list>(check_writes);
		} else {
			return false;
		}
	}

protected:
	// Handle changes when the component pools change
	void process_changes(bool force_rebuild) override {
		if (force_rebuild) {
			do_build();
			return;
		}

		if (!is_enabled()) {
			return;
		}

		if (pools.has_component_count_changed()) {
			do_build();
		}
	}

protected:
	// Number of components
	static constexpr size_t num_components = type_list_size<ComponentsList>;

	// List of components used, with all modifiers stripped
	using stripped_component_list = transform_type<ComponentsList, std::remove_cvref_t>;

	using user_interval = test_option_type_or<is_interval, Options, opts::interval<0, 0>>;
	using interval_type =
		std::conditional_t<(user_interval::_ecs_duration > 0.0),
						   interval_limiter<user_interval::_ecs_duration_ms, user_interval::_ecs_duration_us>, no_interval_limiter>;

	//
	// ecs::parent related stuff

	// The parent type, or void
	using full_parent_type = test_option_type_or<is_parent, stripped_component_list, void>;
	using stripped_parent_type = std::remove_pointer_t<std::remove_cvref_t<full_parent_type>>;
	using parent_component_list = parent_type_list_t<stripped_parent_type>;
	static constexpr bool has_parent_types = !std::is_same_v<full_parent_type, void>;


	// Number of filters
	static constexpr size_t num_filters = count_type_if<ComponentsList>([]<typename T>() { return std::is_pointer_v<T>; });
	static_assert(num_filters < num_components, "systems must have at least one non-filter component");

	// Hashes of stripped types used by this system ('int' instead of 'int const&')
	static constexpr std::array<detail::type_hash, num_components> type_hashes = get_type_hashes_array<stripped_component_list>();

	// The user supplied system
	UpdateFn update_func;

	// Fully typed component pools used by this system
	Pools const pools;

	interval_type interval_checker;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM
#ifndef ECS_SYSTEM_SORTED_H_
#define ECS_SYSTEM_SORTED_H_


namespace ecs::detail {
// Manages sorted arguments. Neither cache- nor storage space friendly, but arguments
// will be passed to the user supplied lambda in a sorted manner
template <typename Options, typename UpdateFn, typename SortFunc, typename TupPools, bool FirstIsEntity, typename ComponentsList>
struct system_sorted final : public system<Options, UpdateFn, TupPools, FirstIsEntity, ComponentsList> {
	using base = system<Options, UpdateFn, TupPools, FirstIsEntity, ComponentsList>;

	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

public:
	system_sorted(UpdateFn func, SortFunc sort, TupPools in_pools)
		: base(func, in_pools), sort_func{sort} {
		this->process_changes(true);
	}

private:
	void do_run() override {
		auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc

		// Sort the arguments if the component data has been modified
		if (needs_sorting || this->pools.template get<sort_types>().has_components_been_modified()) {
			std::sort(e_p, sorted_args.begin(), sorted_args.end(), [this](sort_help const& l, sort_help const& r) {
				return sort_func(*l.sort_val_ptr, *r.sort_val_ptr);
			});

			needs_sorting = false;
		}

		for (sort_help const& sh : sorted_args) {
			lambda_arguments[sh.arg_index](this->update_func, sh.offset);
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build() override {
		sorted_args.clear();
		lambda_arguments.clear();

		for_all_types<ComponentsList>([&]<typename... Types>() {
			find_entity_pool_intersections_cb<ComponentsList>(this->pools, [this, index = 0u](entity_range range) mutable {
				lambda_arguments.push_back(make_argument<Types...>(range, get_component<Types>(range.first(), this->pools)...));

				for (entity_id const entity : range) {
					entity_offset const offset = range.offset(entity);
					sorted_args.push_back({index, offset, get_component<sort_types>(entity, this->pools)});
				}

				index += 1;
			});
		});

		needs_sorting = true;
	}

	template <typename... Ts>
	static auto make_argument(entity_range range, auto... args) {
		return [=](auto update_func, entity_offset offset) {
			entity_id const ent = static_cast<entity_type>(static_cast<entity_offset>(range.first()) + offset);
			if constexpr (FirstIsEntity) {
				update_func(ent, extract_arg_lambda<Ts>(args, offset, 0)...);
			} else {
				update_func(/**/ extract_arg_lambda<Ts>(args, offset, 0)...);
			}
		};
	}

private:
	// The user supplied sorting function
	SortFunc sort_func;

	// The type used for sorting
	using sort_types = sorter_predicate_type_t<SortFunc>;

	// True if the data needs to be sorted
	bool needs_sorting = false;

	struct sort_help {
		unsigned arg_index;
		entity_offset offset;
		sort_types* sort_val_ptr;
	};
	std::vector<sort_help> sorted_args;

	using base_argument = decltype(for_all_types<ComponentsList>([]<typename... Types>() {
			return make_argument<Types...>(entity_range{0,0}, component_argument<Types>{}...);
		}));
	
	std::vector<std::remove_const_t<base_argument>> lambda_arguments;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_SORTED_H_
#ifndef ECS_SYSTEM_RANGED_H_
#define ECS_SYSTEM_RANGED_H_


namespace ecs::detail {
// Manages arguments using ranges. Very fast linear traversal and minimal storage overhead.
template <typename Options, typename UpdateFn, typename Pools, bool FirstIsEntity, typename ComponentsList>
class system_ranged final : public system<Options, UpdateFn, Pools, FirstIsEntity, ComponentsList> {
	using base = system<Options, UpdateFn, Pools, FirstIsEntity, ComponentsList>;

	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

public:
	system_ranged(UpdateFn func, Pools in_pools) : base{func, in_pools} {
		this->process_changes(true);
	}

private:
	void do_run() override {
		// Call the system for all the components that match the system signature
		for (auto& argument : lambda_arguments) {
			argument(this->update_func);
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build() override {
		// Clear current arguments
		lambda_arguments.clear();

		for_all_types<ComponentsList>([&]<typename... Types>() {
			find_entity_pool_intersections_cb<ComponentsList>(this->pools, [this](entity_range found_range) {
				lambda_arguments.push_back(make_argument<Types...>(found_range, get_component<Types>(found_range.first(), this->pools)...));
			});
		});
	}

	template <typename... Ts>
	static auto make_argument(entity_range const range, auto... args) {
		return [=](auto update_func) noexcept {
			auto constexpr e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
			std::for_each(e_p, range.begin(), range.end(), [=](entity_id const ent) mutable noexcept {
				auto const offset = ent - range.first();

				if constexpr (FirstIsEntity) {
					update_func(ent, extract_arg_lambda<Ts>(args, offset, 0)...);
				} else {
					update_func(/**/ extract_arg_lambda<Ts>(args, offset, 0)...);
				}
			});
		};
	}

private:
	/// XXX
	using base_argument = decltype(for_all_types<ComponentsList>([]<typename... Types>() {
			return make_argument<Types...>(entity_range{0,0}, component_argument<Types>{}...);
		}));
	
	std::vector<std::remove_const_t<base_argument>> lambda_arguments;

};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_RANGED_H_
#ifndef ECS_SYSTEM_HIERARCHY_H_
#define ECS_SYSTEM_HIERARCHY_H_


namespace ecs::detail {
template <typename Options, typename UpdateFn, typename Pools, bool FirstIsEntity, typename ComponentsList>
class system_hierarchy final : public system<Options, UpdateFn, Pools, FirstIsEntity, ComponentsList> {
	using base = system<Options, UpdateFn, Pools, FirstIsEntity, ComponentsList>;

	// Is parallel execution wanted
	static constexpr bool is_parallel = !ecs::detail::has_option<opts::not_parallel, Options>();

	struct location {
		std::uint32_t index;
		entity_offset offset;
		auto operator<=>(location const&) const = default;
	};
	struct entity_info {
		std::uint32_t parent_count;
		entity_type root_id;
		location l;

		auto operator<=>(entity_info const&) const = default;
	};
	struct hierarchy_span {
		unsigned offset, count;
	};

public:
	system_hierarchy(UpdateFn func, Pools in_pools) : base{func, in_pools} {
		pool_parent_id = &detail::get_pool<parent_id>(this->pools);
		this->process_changes(true);
	}

private:
	void do_run() override {
		auto const this_pools = this->pools;

		if constexpr (is_parallel) {
			std::for_each(std::execution::par, info_spans.begin(), info_spans.end(), [&](hierarchy_span span) {
				auto const ei_span = std::span<entity_info>{infos.data() + span.offset, span.count};
				for (entity_info const& info : ei_span) {
					arguments[info.l.index](this->update_func, info.l.offset, this_pools);
				}
			});
		} else {
			for (entity_info const& info : infos) {
				arguments[info.l.index](this->update_func, info.l.offset, this_pools);
			}
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build() override {
		ranges.clear();
		ents_to_remove.clear();

		// Find the entities
		find_entity_pool_intersections_cb<ComponentsList>(this->pools, [&](entity_range range) {
			ranges.push_back(range);

			// Get the parent ids in the range
			parent_id const* pid_ptr = pool_parent_id->find_component_data(range.first());

			// the ranges to remove
			for_each_type<parent_component_list>([&]<typename T>() {
				// Get the pool of the parent sub-component
				using X = std::remove_pointer_t<T>;
				component_pool<X> const& sub_pool = this->pools.template get<X>();

				for (size_t pid_index = 0; entity_id const ent : range) {
					parent_id const pid = pid_ptr[pid_index];
					pid_index += 1;

					// Does tests on the parent sub-components to see they satisfy the constraints
					// ie. a 'parent<int*, float>' will return false if the parent does not have a float or
					// has an int.
					if (std::is_pointer_v<T> == sub_pool.has_entity(pid)) {
						// Above 'if' statement is the same as
						//   if (!pointer && !has_entity)	// The type is a filter, so the parent is _not_ allowed to have this component
						//   if (pointer && has_entity)		// The parent must have this component
						merge_or_add(ents_to_remove, entity_range{ent, ent});
					}
				}
			});
		});

		// Remove entities from the result
		ranges = difference_ranges(ranges, ents_to_remove);

		// Clear the arguments
		arguments.clear();
		infos.clear();
		info_spans.clear();

		if (ranges.empty()) {
			return;
		}

		// Build the arguments for the ranges
		for_all_types<ComponentsList>([&]<typename... T>() {
			for (unsigned index = 0; entity_range const range : ranges) {
				arguments.push_back(make_argument<T...>(range, get_component<T>(range.first(), this->pools)...));

				for (entity_id const id : range) {
					infos.push_back({0, *(pool_parent_id->find_component_data(id)), {index, range.offset(id)}});
				}

				index += 1;
			}
		});

		// partition the roots
		auto it = std::partition(infos.begin(), infos.end(), [&](entity_info const& info) {
			return false == pool_parent_id->has_entity(info.root_id);
		});

		// Keep partitioning if there are more levels in the hierarchies
		if (it != infos.begin()) {
			// data needed by the partition lambda
			auto prev_it = infos.begin();
			unsigned hierarchy_level = 1;

			// The lambda used to partion non-root entities
			const auto parter = [&](entity_info& info) {
				// update the parent count while we are here anyway
				info.parent_count = hierarchy_level;

				// Look for the parent in the previous partition
				auto const parent_it = std::find_if(prev_it, it, [&](entity_info const& parent_info) {
					entity_id const parent_id = ranges[parent_info.l.index].at(parent_info.l.offset);
					return parent_id == info.root_id;
				});

				if (it != parent_it) {
					// Propagate the root id downwards to its children
					info.root_id = parent_it->root_id;

					// A parent was found in the previous partition
					return true;
				}
				return false;
			};

			// partition the levels below the roots
			while (it != infos.end()) {
				auto next_it = std::partition(it, infos.end(), parter);

				// Nothing was partitioned, so leave
				if (next_it == it)
					break;

				// Update the partition iterators
				prev_it = it;
				it = next_it;

				hierarchy_level += 1;
			}
		}

		// Do the topological sort of the arguments
		std::sort(infos.begin(), infos.end());

		// The spans are only needed for parallel execution
		if constexpr (is_parallel) {
			// Create the spans
			auto current_root = infos.front().root_id;
			unsigned count = 1;
			unsigned offset = 0;
			
			for (size_t i = 1; i < infos.size(); i++) {
				entity_info const& info = infos[i];
				if (current_root != info.root_id) {
					info_spans.push_back({offset, count});

					current_root = info.root_id;
					offset += count;
					count = 0;
				}

				// TODO compress 'infos' into 'location's inplace

				count += 1;
			}
			info_spans.push_back({offset, static_cast<unsigned>(infos.size() - offset)});
		}
	}

	template <typename... Ts>
	static auto make_argument(entity_range range, auto... args) noexcept {
		return [=](auto update_func, entity_offset offset, auto& pools) mutable {
			entity_id const ent = static_cast<entity_type>(static_cast<entity_offset>(range.first()) + offset);
			if constexpr (FirstIsEntity) {
				update_func(ent, extract_arg_lambda<Ts>(args, offset, pools)...);
			} else {
				update_func(/**/ extract_arg_lambda<Ts>(args, offset, pools)...);
			}
		};
	}

	decltype(auto) make_parent_types_tuple() const {
		return for_all_types<parent_component_list>([this]<typename... T>() {
			return std::make_tuple(&get_pool<std::remove_pointer_t<T>>(this->pools)...);
		});
	}

private:
	using base::has_parent_types;
	using typename base::full_parent_type;
	using typename base::parent_component_list;
	using typename base::stripped_component_list;
	using typename base::stripped_parent_type;

	// The argument for parameter to pass to system func
	using base_argument_ptr = decltype(for_all_types<ComponentsList>([]<typename... Types>() {
		return make_argument<Types...>(entity_range{0, 0}, component_argument<Types>{0}...);
	}));
	using base_argument = std::remove_const_t<base_argument_ptr>;

	// Ensure we have a parent type
	static_assert(has_parent_types, "no parent component found");

	// The vector of entity/parent info
	std::vector<entity_info> infos;

	// The vector of unrolled arguments
	std::vector<base_argument> arguments;

	// The spans over each tree in the argument vector.
	// Only used for parallel execution
	std::vector<hierarchy_span> info_spans;

	std::vector<entity_range> ranges;
	std::vector<entity_range> ents_to_remove;

	// The pool that holds 'parent_id's
	component_pool<parent_id> const* pool_parent_id;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_HIERARCHY_H_
#ifndef ECS_SYSTEM_GLOBAL_H
#define ECS_SYSTEM_GLOBAL_H


namespace ecs::detail {
// The implementation of a system specialized on its components
template <typename Options, typename UpdateFn, typename TupPools, bool FirstIsEntity, typename ComponentsList>
class system_global final : public system<Options, UpdateFn, TupPools, FirstIsEntity, ComponentsList> {
public:
	system_global(UpdateFn func, TupPools in_pools)
		: system<Options, UpdateFn, TupPools, FirstIsEntity, ComponentsList>{func, in_pools} {
		this->process_changes(true);
	  }

private:
	void do_run() override {
		for_all_types<ComponentsList>([&]<typename... Types>(){
			this->update_func(get_pool<Types>(this->pools).get_shared_component()...);
		});
	}

	void do_build() override {
	}
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_GLOBAL_H
#ifndef ECS_SYSTEM_SCHEDULER
#define ECS_SYSTEM_SCHEDULER



namespace ecs::detail {

// Describes a node in the scheduler execution graph
struct scheduler_node final {
	// Construct a node from a system.
	// The system can not be null
	scheduler_node(detail::system_base* _sys) : sys(_sys), dependants{}, unfinished_dependencies{0}, dependencies{0} {
		Expects(sys != nullptr);
	}

	scheduler_node(scheduler_node const& other) {
		sys = other.sys;
		dependants = other.dependants;
		dependencies = other.dependencies;
		unfinished_dependencies = other.unfinished_dependencies.load();
	}

	detail::system_base* get_system() const noexcept {
		return sys;
	}

	// Add a dependant to this system. This system has to run to
	// completion before the dependants can run.
	void add_dependant(size_t node_index) {
		dependants.push_back(node_index);
	}

	// Increase the dependency counter of this system. These dependencies has to
	// run to completion before this system can run.
	void increase_dependency_count() {
		Expects(dependencies != std::numeric_limits<int16_t>::max());
		dependencies += 1;
	}

	// Resets the unfinished dependencies to the total number of dependencies.
	void reset_unfinished_dependencies() {
		unfinished_dependencies = dependencies;
	}

	// Called from systems we depend on when they have run to completion.
	void dependency_done() {
		unfinished_dependencies.fetch_sub(1, std::memory_order_release);
	}

	void run(std::vector<struct scheduler_node>& nodes) {
		// If we are not the last node here, leave
		if (unfinished_dependencies.load(std::memory_order_acquire) != 0)
			return;

		// Run the system
		sys->run();

		// Notify the dependants that we are done
		for (size_t const node : dependants)
			nodes[node].dependency_done();

		// Run the dependants in parallel
		std::for_each(std::execution::par, dependants.begin(), dependants.end(), [&nodes](size_t node) { nodes[node].run(nodes); });
	}

	scheduler_node& operator=(scheduler_node const& other) {
		sys = other.sys;
		dependants = other.dependants;
		dependencies = other.dependencies;
		unfinished_dependencies = other.unfinished_dependencies.load();
		return *this;
	}

private:
	// The system to execute
	detail::system_base* sys{};

	// The systems that depend on this
	std::vector<size_t> dependants{};

	// The number of systems this depends on
	std::atomic<int> unfinished_dependencies = 0;
	int dependencies = 0;
};

// Schedules systems for concurrent execution based on their components.
class scheduler final {
	// A group of systems with the same group id
	struct systems_group final {
		std::vector<scheduler_node> all_nodes;
		std::vector<std::size_t> entry_nodes{};
		int id;

		// Runs the entry nodes in parallel
		void run() {
			std::for_each(std::execution::par, entry_nodes.begin(), entry_nodes.end(),
						  [this](size_t node_id) { all_nodes[node_id].run(all_nodes); });
		}
	};

	std::vector<systems_group> groups;

protected:
	systems_group& find_group(int id) {
		// Look for an existing group
		if (!groups.empty()) {
			for (auto& g : groups) {
				if (g.id == id) {
					return g;
				}
			}
		}

		// No group found, so find an insertion point
		auto const insert_point =
			std::upper_bound(groups.begin(), groups.end(), id, [](int group_id, systems_group const& sg) { return group_id < sg.id; });

		// Insert the group and return it
		return *groups.insert(insert_point, systems_group{{}, {}, id});
	}

public:
	void insert(detail::system_base* sys) {
		// Find the group
		auto& group = find_group(sys->get_group());

		// Create a new node with the system
		size_t const node_index = group.all_nodes.size();
		scheduler_node& node = group.all_nodes.emplace_back(sys);

		// Find a dependant system for each component
		bool inserted = false;
		auto const end = group.all_nodes.rend();
		for (auto const hash : sys->get_type_hashes()) {
			auto it = std::next(group.all_nodes.rbegin()); // 'next' to skip the newly added system
			while (it != end) {
				scheduler_node& dep_node = *it;
				// If the other system doesn't touch the same component,
				// then there can be no dependecy
				if (dep_node.get_system()->has_component(hash)) {
					if (dep_node.get_system()->writes_to_component(hash) || sys->writes_to_component(hash)) {
						// The system writes to the component,
						// so there is a strong dependency here.
						inserted = true;
						dep_node.add_dependant(node_index);
						node.increase_dependency_count();
						break;
					} else { // 'other' reads component
							 // These systems have a weak read/read dependency
							 // and can be scheduled concurrently
					}
				}

				++it;
			}
		}

		// The system has no dependencies, so make it an entry node
		if (!inserted) {
			group.entry_nodes.push_back(node_index);
		}
	}

	// Clears all the schedulers data
	void clear() {
		groups.clear();
	}

	void run() {
		// Reset the execution data
		for (auto& group : groups) {
			for (auto& node : group.all_nodes)
				node.reset_unfinished_dependencies();
		}

		// Run the groups in succession
		for (auto& group : groups) {
			group.run();
		}
	}
};

} // namespace ecs::detail

#endif // !ECS_SYSTEM_SCHEDULER
#ifndef ECS_CONTEXT
#define ECS_CONTEXT




namespace ecs::detail {
// The central class of the ecs implementation. Maintains the state of the system.
class context final {
	// The values that make up the ecs core.
	std::vector<std::unique_ptr<system_base>> systems;
	std::vector<std::unique_ptr<component_pool_base>> component_pools;
	std::map<type_hash, component_pool_base*> type_pool_lookup; // TODO vector
	tls::split<tls::cache<type_hash, component_pool_base*, get_type_hash<void>()>> type_caches;
	scheduler sched;

	mutable std::shared_mutex system_mutex;
	mutable std::shared_mutex component_pool_mutex;

public:
	// Commits the changes to the entities.
	void commit_changes() {
		// Prevent other threads from
		//  adding components
		//  registering new component types
		//  adding new systems
		std::shared_lock system_lock(system_mutex, std::defer_lock);
		std::unique_lock component_pool_lock(component_pool_mutex, std::defer_lock);
		std::lock(system_lock, component_pool_lock); // lock both without deadlock

		static constexpr auto process_changes = [](auto const& inst) {
			inst->process_changes();
		};

		// Let the component pools handle pending add/remove requests for components
		std::for_each(std::execution::par, component_pools.begin(), component_pools.end(), process_changes);

		// Let the systems respond to any changes in the component pools
		std::for_each(std::execution::par, systems.begin(), systems.end(), process_changes);

		// Reset any dirty flags on pools
		for (auto const& pool : component_pools) {
			pool->clear_flags();
		}
	}

	// Calls the 'update' function on all the systems in the order they were added.
	void run_systems() {
		// Prevent other threads from adding new systems during the run
		std::shared_lock system_lock(system_mutex);

		// Run all the systems
		sched.run();
	}

	// Returns true if a pool for the type exists
	template <typename T>
	bool has_component_pool() const {
		// Prevent other threads from registering new component types
		std::shared_lock component_pool_lock(component_pool_mutex);

		static constexpr auto hash = get_type_hash<T>();
		return type_pool_lookup.contains(hash);
	}

	// Resets the runtime state. Removes all systems, empties component pools
	void reset() {
		std::unique_lock system_lock(system_mutex, std::defer_lock);
		std::unique_lock component_pool_lock(component_pool_mutex, std::defer_lock);
		std::lock(system_lock, component_pool_lock); // lock both without deadlock

		systems.clear();
		sched.clear();
		type_pool_lookup.clear();
		component_pools.clear();
		type_caches.reset();
	}

	// Returns a reference to a components pool.
	// If a pool doesn't exist, one will be created.
	template <typename T>
	auto& get_component_pool() {
		// This assert is here to prevent calls like get_component_pool<T> and get_component_pool<T&>,
		// which will produce the exact same code. It should help a bit with compilation times
		// and prevent the compiler from generating duplicated code.
		static_assert(std::is_same_v<T, std::remove_pointer_t<std::remove_cvref_t<T>>>,
					  "This function only takes naked types, like 'int', and not 'int const&' or 'int*'");

		auto& cache = type_caches.local();

		static constexpr auto hash = get_type_hash<T>();
		auto pool = cache.get_or(hash, [this](type_hash _hash) {
			// A new pool might be created, so take a unique lock
			std::unique_lock component_pool_lock(component_pool_mutex);

			// Look in the pool for the type
			auto const it = type_pool_lookup.find(_hash);
			if (it == type_pool_lookup.end()) {
				// The pool wasn't found so create it.
				return create_component_pool<T>();
			} else {
				return it->second;
			}
		});

		return *static_cast<component_pool<T>*>(pool);
	}

	// Regular function
	template <typename Options, typename UpdateFn, typename SortFn, typename R, typename FirstArg, typename... Args>
	decltype(auto) create_system(UpdateFn update_func, SortFn sort_func, R(FirstArg, Args...)) {
		return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(update_func, sort_func);
	}

	// Const lambda with sort
	template <typename Options, typename UpdateFn, typename SortFn, typename R, typename C, typename FirstArg, typename... Args>
	decltype(auto) create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstArg, Args...) const) {
		return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(update_func, sort_func);
	}

	// Mutable lambda with sort
	template <typename Options, typename UpdateFn, typename SortFn, typename R, typename C, typename FirstArg, typename... Args>
	decltype(auto) create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstArg, Args...)) {
		return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(update_func, sort_func);
	}

private:
	template<typename T>
	using stripper = reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<T>>>;

	template <impl::TypeList ComponentList>
	auto make_pools() {
		using stripped_list = transform_type<ComponentList, stripper>;

		return for_all_types<stripped_list>([this]<typename... Types>() {
			return detail::component_pools<stripped_list>{
				&this->get_component_pool<Types>()...};
		});
	}

	template <typename Options, typename UpdateFn, typename SortFn, typename FirstComponent, typename... Components>
	decltype(auto) create_system(UpdateFn update_func, SortFn sort_func) {
		// Is the first component an entity_id?
		static constexpr bool first_is_entity = is_entity<FirstComponent>;

		// The type_list of components
		using component_list = std::conditional_t<first_is_entity, type_list<Components...>, type_list<FirstComponent, Components...>>;
	
		// Find potential parent type
		using parent_type = test_option_type_or<is_parent, component_list, void>;

		// Do some checks on the systems
		static bool constexpr has_sort_func = !std::is_same_v<SortFn, std::nullptr_t>;
		static bool constexpr has_parent = !std::is_same_v<void, parent_type>;
		static bool constexpr is_global_sys = for_all_types<component_list>([]<typename... Types>() {
				return (detail::global<Types> && ...);
			});

		// Global systems cannot have a sort function
		static_assert(!(is_global_sys == has_sort_func && is_global_sys), "Global systems can not be sorted");

		static_assert(!(has_sort_func == has_parent && has_parent == true), "Systems can not both be hierarchial and sorted");

		// Helper-lambda to insert system
		auto const insert_system = [this](auto& system) -> decltype(auto) {
			std::unique_lock system_lock(system_mutex);

			[[maybe_unused]] auto sys_ptr = system.get();

			systems.push_back(std::move(system));
			detail::system_base* ptr_system = systems.back().get();
			Ensures(ptr_system != nullptr);

			// -vv-  msvc shenanigans
			[[maybe_unused]] static bool constexpr request_manual_update = has_option<opts::manual_update, Options>();
			if constexpr (!request_manual_update) {
				sched.insert(ptr_system);
			} else {
				return (*sys_ptr);
			}
		};

		// Create the system instance
		if constexpr (has_parent) {
			auto const pools = make_pools<detail::merge_type_lists<component_list, parent_type_list_t<parent_type>>>();
			using typed_system = system_hierarchy<Options, UpdateFn, decltype(pools), first_is_entity, component_list>;
			auto sys = std::make_unique<typed_system>(update_func, pools);
			return insert_system(sys);
		} else if constexpr (is_global_sys) {
			auto const pools = make_pools<component_list>();
			using typed_system = system_global<Options, UpdateFn, decltype(pools), first_is_entity, component_list>;
			auto sys = std::make_unique<typed_system>(update_func, pools);
			return insert_system(sys);
		} else if constexpr (has_sort_func) {
			auto const pools = make_pools<component_list>();
			using typed_system = system_sorted<Options, UpdateFn, SortFn, decltype(pools), first_is_entity, component_list>;
			auto sys = std::make_unique<typed_system>(update_func, sort_func, pools);
			return insert_system(sys);
		} else {
			auto const pools = make_pools<component_list>();
			using typed_system = system_ranged<Options, UpdateFn, decltype(pools), first_is_entity, component_list>;
			auto sys = std::make_unique<typed_system>(update_func, pools);
			return insert_system(sys);
		}
	}

	// Create a component pool for a new type
	template <typename T>
	component_pool_base* create_component_pool() {
		// Create a new pool
		auto pool = std::make_unique<component_pool<T>>();
		static constexpr auto hash = get_type_hash<T>();
		type_pool_lookup.emplace(hash, pool.get());
		component_pools.push_back(std::move(pool));
		return component_pools.back().get();
	}
};
} // namespace ecs::detail

#endif // !ECS_CONTEXT
#ifndef ECS_RUNTIME
#define ECS_RUNTIME



namespace ecs {
class runtime {
public:
	// Add several components to a range of entities. Will not be added until 'commit_changes()' is called.
	// Pre: entity does not already have the component, or have it in queue to be added
	template <typename First, typename... T>
	constexpr void add_component(entity_range const range, First&& first_val, T&&... vals) {
		static_assert(detail::is_unique_types<First, T...>(), "the same component was specified more than once");
		static_assert(!detail::global<First> && (!detail::global<T> && ...), "can not add global components to entities");
		static_assert(!std::is_pointer_v<std::remove_cvref_t<First>> && (!std::is_pointer_v<std::remove_cvref_t<T>> && ...),
					  "can not add pointers to entities; wrap them in a struct");

		auto const adder = [this, range]<typename Type>(Type&& val) {
			// Add it to the component pool
			if constexpr (detail::is_parent<std::remove_cvref_t<Type>>::value) {
				auto& pool = ctx.get_component_pool<detail::parent_id>();
				pool.add(range, detail::parent_id{val.id()});
			} else if constexpr (std::is_reference_v<Type>) {
				using DerefT = std::remove_cvref_t<Type>;
				static_assert(std::copyable<DerefT>, "Type must be copyable");

				detail::component_pool<DerefT>& pool = ctx.get_component_pool<DerefT>();
				pool.add(range, val);
			} else {
				static_assert(std::copyable<Type>, "Type must be copyable");

				detail::component_pool<Type>& pool = ctx.get_component_pool<Type>();
				pool.add(range, std::forward<Type>(val));
			}
		};

		adder(std::forward<First>(first_val));
		(adder(std::forward<T>(vals)), ...);
	}

	// Adds a span of components to a range of entities. Will not be added until 'commit_changes()' is called.
	// Pre: entity does not already have the component, or have it in queue to be added
	void add_component_span(entity_range const range, std::ranges::contiguous_range auto const& vals) {
		using T = typename std::remove_cvref_t<decltype(vals)>::value_type;
		static_assert(!detail::global<T>, "can not add global components to entities");
		static_assert(!std::is_pointer_v<std::remove_cvref_t<T>>, "can not add pointers to entities; wrap them in a struct");
		//static_assert(!detail::is_parent<std::remove_cvref_t<T>>::value, "adding spans of parents is not (yet?) supported"); // should work
		static_assert(std::copyable<T>, "Type must be copyable");

		// Add it to the component pool
		detail::component_pool<T>& pool = ctx.get_component_pool<T>();
		pool.add_span(range, std::span{vals});
	}

	template <typename Fn>
	void add_component_generator(entity_range const range, Fn&& gen) {
		// Return type of 'func'
		using ComponentType = decltype(std::declval<Fn>()(entity_id{0}));
		static_assert(!std::is_same_v<ComponentType, void>, "Initializer functions must return a component");

		if constexpr (detail::is_parent<std::remove_cvref_t<ComponentType>>::value) {
			auto const converter = [gen = std::forward<Fn>(gen)](entity_id id) {
				return detail::parent_id{gen(id).id()};
			};

			auto& pool = ctx.get_component_pool<detail::parent_id>();
			pool.add_generator(range, converter);
		} else {
			auto& pool = ctx.get_component_pool<ComponentType>();
			pool.add_generator(range, std::forward<Fn>(gen));
		}
	}

	// Add several components to an entity. Will not be added until 'commit_changes()' is called.
	// Pre: entity does not already have the component, or have it in queue to be added
	template <typename First, typename... T>
	void add_component(entity_id const id, First&& first_val, T&&... vals) {
		add_component(entity_range{id, id}, std::forward<First>(first_val), std::forward<T>(vals)...);
	}

	// Removes a component from a range of entities. Will not be removed until 'commit_changes()' is
	// called. Pre: entity has the component
	template <detail::persistent T>
	void remove_component(entity_range const range, T const& = T{}) {
		static_assert(!detail::global<T>, "can not remove or add global components to entities");

		// Remove the entities from the components pool
		detail::component_pool<T>& pool = ctx.get_component_pool<T>();
		pool.remove(range);
	}

	// Removes a component from an entity. Will not be removed until 'commit_changes()' is called.
	// Pre: entity has the component
	template <typename T>
	void remove_component(entity_id const id, T const& = T{}) {
		remove_component<T>({id, id});
	}

	// Returns a global component.
	template <detail::global T>
	T& get_global_component() {
		return ctx.get_component_pool<T>().get_shared_component();
	}

	// Returns the component from an entity, or nullptr if the entity is not found
	// NOTE: Pointers to components are only guaranteed to be valid
	//       until the next call to 'ecs::commit_changes' or 'ecs::update',
	//       after which the component might be reallocated.
	template <detail::local T>
	T* get_component(entity_id const id) {
		// Get the component pool
		detail::component_pool<T>& pool = ctx.get_component_pool<T>();
		return pool.find_component_data(id);
	}

	// Returns the components from an entity range, or an empty span if the entities are not found
	// or does not containg the component.
	// NOTE: Pointers to components are only guaranteed to be valid
	//       until the next call to ecs::commit_changes or ecs::update,
	//       after which the component might be reallocated.
	template <detail::local T>
	std::span<T> get_components(entity_range const range) {
		if (!has_component<T>(range))
			return {};

		// Get the component pool
		detail::component_pool<T>& pool = ctx.get_component_pool<T>();
		return {pool.find_component_data(range.first()), range.ucount()};
	}

	// Returns the number of active components for a specific type of components
	template <typename T>
	ptrdiff_t get_component_count() {
		// Get the component pool
		detail::component_pool<T> const& pool = ctx.get_component_pool<T>();
		return pool.num_components();
	}

	// Returns the number of entities that has the component.
	template <typename T>
	ptrdiff_t get_entity_count() {
		// Get the component pool
		detail::component_pool<T> const& pool = ctx.get_component_pool<T>();
		return pool.num_entities();
	}

	// Return true if an entity contains the component
	template <typename T>
	bool has_component(entity_id const id) {
		detail::component_pool<T> const& pool = ctx.get_component_pool<T>();
		return pool.has_entity(id);
	}

	// Returns true if all entities in a range has the component.
	template <typename T>
	bool has_component(entity_range const range) {
		detail::component_pool<T>& pool = ctx.get_component_pool<T>();
		return pool.has_entity(range);
	}

	// Commits the changes to the entities.
	inline void commit_changes() {
		ctx.commit_changes();
	}

	// Calls the 'update' function on all the systems in the order they were added.
	inline void run_systems() {
		ctx.run_systems();
	}

	// Commits all changes and calls the 'update' function on all the systems in the order they were
	// added. Same as calling commit_changes() and run_systems().
	inline void update() {
		commit_changes();
		run_systems();
	}

	// Make a new system
	template <typename... Options, typename SystemFunc, typename SortFn = std::nullptr_t>
	decltype(auto) make_system(SystemFunc sys_func, SortFn sort_func = nullptr) {
		using opts = detail::type_list<Options...>;

		// verify the input
		detail::make_system_parameter_verifier<opts, SystemFunc, SortFn>();

		if constexpr (ecs::detail::type_is_function<SystemFunc>) {
			// Build from regular function
			return ctx.create_system<opts, SystemFunc, SortFn>(sys_func, sort_func, sys_func);
		} else if constexpr (ecs::detail::type_is_lambda<SystemFunc>) {
			// Build from lambda
			return ctx.create_system<opts, SystemFunc, SortFn>(sys_func, sort_func, &SystemFunc::operator());
		} else {
			(void)sys_func;
			(void)sort_func;
			struct _invalid_system_type {
			} invalid_system_type;
			return invalid_system_type;
		}
	}

	// Set the memory resource to use to store a specific type of component
	/*template <typename Component>
	void set_memory_resource(std::pmr::memory_resource* resource) {
		auto& pool = ctx.get_component_pool<Component>();
		pool.set_memory_resource(resource);
	}

	// Returns the memory resource used to store a specific type of component
	template <typename Component>
	std::pmr::memory_resource* get_memory_resource() {
		auto& pool = ctx.get_component_pool<Component>();
		return pool.get_memory_resource();
	}

	// Resets the memory resource to the default
	template <typename Component>
	void reset_memory_resource() {
		auto& pool = ctx.get_component_pool<Component>();
		pool.set_memory_resource(std::pmr::get_default_resource());
	}*/

private:
	detail::context ctx;
};
} // namespace ecs

#endif // !ECS_RUNTIME

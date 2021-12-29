// Auto-generated single-header include file
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <execution>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>


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
		constexpr ~thread_data() noexcept {
			if (owner != nullptr) {
				owner->remove_thread(this);
			}
		}

		// Return a reference to an instances local data
		[[nodiscard]] constexpr T& get(split* instance) noexcept {
			// If the owner is null, (re-)initialize the instance.
			// Data may still be present if the thread_local instance is still active
			if (owner == nullptr) {
				data = {};
				owner = instance;
				instance->init_thread(this);
			}
			return data;
		}

		constexpr void remove(split* instance) noexcept {
			if (owner == instance) {
				data = {};
				owner = nullptr;
				next = nullptr;
			}
		}

		[[nodiscard]] constexpr T* get_data() noexcept {
			return &data;
		}
		[[nodiscard]] constexpr T const* get_data() const noexcept {
			return &data;
		}

		constexpr void set_next(thread_data* ia) noexcept {
			next = ia;
		}
		[[nodiscard]] constexpr thread_data* get_next() noexcept {
			return next;
		}
		[[nodiscard]] constexpr thread_data const* get_next() const noexcept {
			return next;
		}

	private:
		T data{};
		split<T, UnusedDifferentiaterType> *owner{};
		thread_data *next = nullptr;
	};

private:
	// the head of the threads that access this split instance
	thread_data *head{};

	// Mutex for serializing access for adding/removing thread-local instances
	std::mutex* mtx_storage{};

	// Data that is only used in constexpr evaluations
	thread_data consteval_data{};

protected:
	[[nodiscard]] std::mutex& get_runtime_mutex() noexcept {
		return *mtx_storage;
	}

	// Adds a thread_data
	constexpr void init_thread(thread_data* t) noexcept {
		auto const init_thread_imp = [&]() noexcept {
			t->set_next(head);
			head = t;
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			init_thread_imp();
		} else {
			init_thread_imp();
		}
	}

	// Remove the thread_data
	constexpr void remove_thread(thread_data* t) noexcept {
		auto const remove_thread_impl = [&]() noexcept {
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
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			remove_thread_impl();
		} else {
			remove_thread_impl();
		}
	}

public:
	constexpr split() noexcept {
		if (!std::is_constant_evaluated())
			mtx_storage = new std::mutex;
	}
	constexpr split(split const&) = delete;
	constexpr split(split&&) noexcept = default;
	constexpr split& operator=(split const&) = delete;
	constexpr split& operator=(split&&) noexcept = default;
	constexpr ~split() noexcept {
		reset();

		if (!std::is_constant_evaluated())
			delete mtx_storage;
	}

	// Get the thread-local instance of T
	constexpr T& local() noexcept {
		if (!std::is_constant_evaluated()) {
			auto const local_impl = [&]() -> T& {
				thread_local thread_data var{};
				return var.get(this);
			};
			return local_impl();
		} else {
			return consteval_data.get(this);
		}
	}

	// Performa an action on all each instance of the data
	template<class Fn>
	constexpr void for_each(Fn&& fn) {
		auto const for_each_impl = [&]() noexcept {
			for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
				fn(*thread->get_data());
			}
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			for_each_impl();
		} else {
			for_each_impl();
		}
	}

	// Resets all data and threads
	constexpr void reset() noexcept {
		auto const impl = [&] {
			for (thread_data* instance = head; instance != nullptr;) {
				auto next = instance->get_next();
				instance->remove(this);
				instance = next;
			}

			head = nullptr;
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			impl();
		} else {
			impl();
		}
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
		constexpr ~thread_data() noexcept {
			if (owner != nullptr) {
				owner->remove_thread(this);
			}
		}

		// Return a reference to an instances local data
		[[nodiscard]] constexpr T& get(collect* instance) noexcept {
			// If the owner is null, (re-)initialize the thread.
			// Data may still be present if the thread_local instance is still active
			if (owner == nullptr) {
				data = {};
				owner = instance;
				instance->init_thread(this);
			}
			return data;
		}

		constexpr void remove(collect* instance) noexcept {
			if (owner == instance) {
				data = {};
				owner = nullptr;
				next = nullptr;
			}
		}

		[[nodiscard]] constexpr T* get_data() noexcept {
			return &data;
		}
		[[nodiscard]] constexpr T const* get_data() const noexcept {
			return &data;
		}

		constexpr void set_next(thread_data* ia) noexcept {
			next = ia;
		}
		[[nodiscard]] constexpr thread_data* get_next() noexcept {
			return next;
		}
		[[nodiscard]] constexpr thread_data const* get_next() const noexcept {
			return next;
		}

	private:
		T data{};
		collect* owner{};
		thread_data* next = nullptr;
	};

private:
	// the head of the threads that access this splitter instance
	thread_data* head{};

	// All the data collected from threads
	std::vector<T> data{};

	// Mutex for serializing access for adding/removing thread-local instances
	std::mutex* mtx_ptr{};

	// Data that is only used in constexpr evaluations
	thread_data consteval_data;

	[[nodiscard]] std::mutex& get_runtime_mutex() noexcept {
		return *mtx_ptr;
	}

	// Adds a new thread
	constexpr void init_thread(thread_data* t) noexcept {
		auto const init_thread_imp = [&]() noexcept {
			t->set_next(head);
			head = t;
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			init_thread_imp();
		} else {
			init_thread_imp();
		}
	}

	// Removes the thread
	constexpr void remove_thread(thread_data* t) noexcept {
		auto const remove_thread_impl = [&]() noexcept {
			// Take the thread data
			T* local_data = t->get_data();
			data.push_back(std::move(*local_data));

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
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			remove_thread_impl();
		} else {
			remove_thread_impl();
		}
	}

public:
	constexpr collect() noexcept {
		if (!std::is_constant_evaluated())
			mtx_ptr = new std::mutex{};
	}
	constexpr collect(collect const&) = delete;
	constexpr collect(collect&&) noexcept = default;
	constexpr collect& operator=(collect const&) = delete;
	constexpr collect& operator=(collect&&) noexcept = default;
	constexpr ~collect() noexcept {
		reset();

		if (!std::is_constant_evaluated())
			delete mtx_ptr;
	}

	// Get the thread-local thread of T
	[[nodiscard]] constexpr T& local() noexcept {
		if (!std::is_constant_evaluated()) {
			auto const local_impl = [&]() -> T& {
				thread_local thread_data var{};
				return var.get(this);
			};
			return local_impl();
		} else {
			return consteval_data.get(this);
		}
	}

	// Gathers all the threads data and returns it. This clears all stored data.
	[[nodiscard]] constexpr std::vector<T> gather() noexcept {
		auto const gather_impl = [&]() noexcept {
			for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
				data.push_back(std::move(*thread->get_data()));
				*thread->get_data() = T{};
			}

			return std::move(data);
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			return gather_impl();
		} else {
			return gather_impl();
		}
	}

	// Gathers all the threads data and sends it to the output iterator. This clears all stored data.
	constexpr void gather_flattened(auto dest_iterator) noexcept {
		auto const gather_flattened_impl = [&]() noexcept {
			for (T& t : data) {
				std::move(t.begin(), t.end(), dest_iterator);
			}
			data.clear();

			for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
				T* ptr_t = thread->get_data();
				std::move(ptr_t->begin(), ptr_t->end(), dest_iterator);
				*ptr_t = T{};
			}
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			gather_flattened_impl();
		} else {
			gather_flattened_impl();
		}
	}

	// Perform an action on all threads data
	template <class Fn>
	constexpr void for_each(Fn&& fn) noexcept {
		auto const for_each_impl = [&]() noexcept {
			for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
				fn(*thread->get_data());
			}

			std::for_each(data.begin(), data.end(), std::forward<Fn>(fn));
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			for_each_impl();
		} else {
			for_each_impl();
		}
	}

	// Resets all data and threads
	constexpr void reset() noexcept {
		auto const reset_impl = [&]() noexcept {
			for (thread_data* thread = head; thread != nullptr;) {
				auto next = thread->get_next();
				thread->remove(this);
				thread = next;
			}

			head = nullptr;
			data.clear();
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			reset_impl();
		} else {
			reset_impl();
		}
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

namespace impl {
// Implementation of type_list_size.
template <typename>
struct type_list_size;
template <>
struct type_list_size<void> {
	static constexpr size_t value = 0;
};
template <typename... Types>
struct type_list_size<type_list<Types...>> {
	static constexpr size_t value = sizeof...(Types);
};

// Implementation of type_list_at.
template <int, typename>
struct type_list_at;
template <int I, typename Type, typename... Types>
struct type_list_at<I, type_list<Type, Types...>> {
	using type = typename type_list_at<I - 1, type_list<Types...>>::type;
};
template <typename Type, typename... Types>
struct type_list_at<0, type_list<Type, Types...>> {
	using type = Type;
};

// Implementation of type_list_at_or.
template <int, typename OrType, typename TypeList>
struct type_list_at_or;

template <int I, typename OrType, typename Type, typename... Types>
struct type_list_at_or<I, OrType, type_list<Type, Types...>> {
	using type = typename type_list_at_or<I - 1, OrType, type_list<Types...>>::type;
};

template <typename Type, typename OrType, typename... Types>
struct type_list_at_or<0, OrType, type_list<Type, Types...>> {
	using type = Type;
};

template <typename Type, typename OrType, typename... Types>
struct type_list_at_or<int{-1}, OrType, type_list<Type, Types...>> {
	using type = OrType;
};

template <typename Type, typename F>
constexpr decltype(auto) invoke_type(F&& f) {
	return f.template operator()<Type>();
}

template <typename... Types, typename F>
constexpr void for_each_type(F&& f, type_list<Types...>*) {
	(invoke_type<Types>(f), ...);
}

template <typename... Types, typename F>
constexpr decltype(auto) apply_type(F&& f, type_list<Types...>*) {
	return f.template operator()<Types...>();
}

template <typename... Types, typename F>
constexpr bool all_of_type(F&& f, type_list<Types...>*) {
	return (invoke_type<Types>(f) && ...);
}

template <typename... Types, typename F>
constexpr bool any_of_type(F&& f, type_list<Types...>*) {
	return (invoke_type<Types>(f) || ...);
}
} // namespace impl

template <typename Types>
constexpr size_t type_list_size = impl::type_list_size<Types>::value;

template <int I, typename Types>
using type_list_at = typename impl::type_list_at<I, Types>::type;

template <int I, typename Types, typename OrType>
using type_list_at_or = typename impl::type_list_at_or<I, OrType, Types>::type;

// Applies the functor F to each type in the type list.
// Takes lambdas of the form '[]<typename T>() {}'
template <typename TypeList, typename F>
constexpr void for_each_type(F&& f) {
	impl::for_each_type(f, static_cast<TypeList*>(nullptr));
}

// Applies the functor F to all types in the type list.
// Takes lambdas of the form '[]<typename ...T>() {}'
template <typename TypeList, typename F>
constexpr decltype(auto) apply_type(F&& f) {
	return impl::apply_type(f, static_cast<TypeList*>(nullptr));
}

// Applies the bool-returning functor F to each type in the type list.
// Returns true if all of them return true.
// Takes lambdas of the form '[]<typename T>() -> bool {}'
template <typename TypeList, typename F>
constexpr bool all_of_type(F&& f) {
	return impl::all_of_type(f, static_cast<TypeList*>(nullptr));
}

// Applies the bool-returning functor F to each type in the type list.
// Returns true if any of them return true.
// Takes lambdas of the form '[]<typename T>() -> bool {}'
template <typename TypeList, typename F>
constexpr bool any_of_type(F&& f) {
	return impl::any_of_type(f, static_cast<TypeList*>(nullptr));
}

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

template <class T>
constexpr auto get_type_name() {
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

template <class T>
constexpr type_hash get_type_hash() {
	constexpr type_hash prime = 0x100000001b3;
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

template <bool ignore_first_arg, typename First, typename... Types>
constexpr auto get_type_hashes_array() {
	if constexpr (!ignore_first_arg) {
		std::array<detail::type_hash, 1 + sizeof...(Types)> arr{get_type_hash<First>(), get_type_hash<Types>()...};
		return arr;
	} else {
		std::array<detail::type_hash, sizeof...(Types)> arr{get_type_hash<Types>()...};
		return arr;
	}
}

} // namespace ecs::detail

#endif // !ECS_TYPE_HASH
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
#ifndef ECS_PARENT_H_
#define ECS_PARENT_H_


namespace ecs::detail {
template <typename Component, typename Pools>
auto get_component(entity_id const, Pools const&);

template <class Pools>
struct pool_entity_walker;
} // namespace ecs::detail

namespace ecs {

// Special component that allows parent/child relationships
template <typename... ParentTypes>
struct parent : entity_id {

	explicit parent(entity_id id) : entity_id(id) {}

	parent(parent const&) = default;
	parent& operator=(parent const&) = default;

	entity_id id() const {
		return (entity_id) * this;
	}

	template <typename T>
	T& get() {
		static_assert((std::is_same_v<T, ParentTypes> || ...), "T is not specified in the parent component");
		return *std::get<T*>(parent_components);
	}

	template <typename T>
	T const& get() const {
		static_assert((std::is_same_v<T, ParentTypes> || ...), "T is not specified in the parent component");
		return *std::get<T*>(parent_components);
	}

	// used internally by detectors
	struct _ecs_parent {};

private:
	template <typename Component, typename Pools>
	friend auto detail::get_component(entity_id const, Pools const&);

	template <class Pools>
	friend struct detail::pool_entity_walker;

	parent(entity_id id, std::tuple<ParentTypes*...> tup)
		: entity_id(id)
		, parent_components(tup) {
	}

	std::tuple<ParentTypes*...> parent_components;
};
} // namespace ecs

#endif // !ECS_PARENT_H_
#ifndef ECS_DETAIL_PARENT_H
#define ECS_DETAIL_PARENT_H


namespace ecs::detail {

// The parent type stored internally in component pools
struct parent_id : entity_id {};

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
requires requires {
	typename T::_ecs_parent;
}
struct is_parent<T> {
	static constexpr bool value = true;
};

// Contains detectors for the options
namespace detect {
template <int Index, template <class O> class Tester, class ListOptions>
constexpr int find_tester_index() {
	if constexpr (Index == type_list_size<ListOptions>) {
		return -1; // type not found
	} else if constexpr (Tester<type_list_at<Index, ListOptions>>::value) {
		return Index;
	} else {
		return find_tester_index<Index + 1, Tester, ListOptions>();
	}
}

template <int Index, typename Option, class ListOptions>
constexpr int find_type_index() {
	if constexpr (Index == type_list_size<ListOptions>) {
		return -1; // type not found
	} else if constexpr (std::is_same_v<Option, type_list_at<Index, ListOptions>>) {
		return Index;
	} else {
		return find_type_index<Index + 1, Option, ListOptions>();
	}
}

// A detector that applies Tester to each option.
template <template <class O> class Tester, class ListOptions, class NotFoundType = void>
constexpr auto test_option() {
	if constexpr (type_list_size<ListOptions> == 0) {
		return (NotFoundType*)0;
	} else {
		constexpr int option_index = find_tester_index<0, Tester, ListOptions>();
		if constexpr (option_index != -1) {
			using opt_type = type_list_at<option_index, ListOptions>;
			return (opt_type*)0;
		} else {
			return (NotFoundType*)0;
		}
	}
}

template <class Option, class ListOptions>
constexpr bool has_option() {
	if constexpr (type_list_size<ListOptions> == 0) {
		return false;
	} else {
		constexpr int option_index = find_type_index<0, Option, ListOptions>();
		return option_index != -1;
	}
}
} // namespace detect

// Use a tester to check the options. Takes a tester structure and a tuple of options to test against.
// The tester must have static member 'value' that determines if the option passed to it
// is what it is looking for, see 'is_group' for an example.
// STL testers like 'std::is_execution_policy' can also be used
template <template <class O> class Tester, class TupleOptions>
using test_option_type = std::remove_pointer_t<decltype(detect::test_option<Tester, TupleOptions>())>;

// Use a tester to check the options. Results in 'NotFoundType' if the tester
// does not find a viable option.
template <template <class O> class Tester, class TupleOptions, class NotFoundType>
using test_option_type_or = std::remove_pointer_t<decltype(detect::test_option<Tester, TupleOptions, NotFoundType>())>;

// Use a tester to find the index of a type in the tuple. Results in -1 if not found
template <template <class O> class Tester, class TupleOptions>
static constexpr int test_option_index = detect::find_tester_index<0, Tester, TupleOptions>();

template <class Option, class TupleOptions>
constexpr bool has_option() {
	return detect::has_option<Option, TupleOptions>();
}

} // namespace ecs::detail

#endif // !ECS_DETAIL_OPTIONS_H
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

constexpr static std::size_t parallelization_size_tipping_point = 4096;

template <class ForwardIt, class BinaryPredicate>
constexpr ForwardIt std_combine_erase(ForwardIt first, ForwardIt last, BinaryPredicate&& p) noexcept {
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

template <class Cont, class BinaryPredicate>
constexpr void combine_erase(Cont& cont, BinaryPredicate&& p) noexcept {
	auto const end = std_combine_erase(cont.begin(), cont.end(), static_cast<BinaryPredicate&&>(p));
	cont.erase(end, cont.end());
}

template <typename T, typename Alloc = std::allocator<T>>
class component_pool final : public component_pool_base {
private:
	static_assert(!is_parent<T>::value, "can not have pools of any ecs::parent<type>");

	using allocator_type = Alloc;

	struct chunk {
		// The full range this chunk covers.
		entity_range range;

		// The partial range of active entities inside this chunk
		entity_range active;

		// The data for the full range of the chunk (range.count())
		// The tag signals if this chunk owns this data and should clean it up
		T* data = nullptr;

		// Points to the next chunk in the list.
		// The tag signals if this chunk has been split
		chunk* next = nullptr;

		bool owns_data = false;
		bool has_split_data = false;
	};
	//static_assert(sizeof(chunk) == 32);

	allocator_type alloc;
	std::allocator<chunk> alloc_chunk;

	chunk* head = nullptr;

	// Keep track of which components to add/remove each cycle
	using entity_data = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, T>>;
	using entity_span = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, std::span<const T>>>;
	tls::collect<std::vector<entity_data>, component_pool<T>> deferred_adds;
	tls::collect<std::vector<entity_span>, component_pool<T>> deferred_spans;
	tls::collect<std::vector<entity_range>, component_pool<T>> deferred_removes;

	std::vector<entity_range> ordered_active_ranges;
	std::vector<chunk*> ordered_chunks;

	// Status flags
	bool components_added = false;
	bool components_removed = false;
	bool components_modified = false;

public:
	constexpr component_pool() noexcept {
		if constexpr (global<T>) {
			head = create_new_chunk({0, 0}, {0, 0});
			head->data = alloc.allocate(1);
			std::construct_at(head->data);
			ordered_active_ranges.push_back(entity_range::all());
			ordered_chunks.push_back(head);
		}
	}
	constexpr component_pool(component_pool const&) = delete;
	constexpr component_pool(component_pool&&) = delete;
	constexpr component_pool& operator=(component_pool const&) = delete;
	constexpr component_pool& operator=(component_pool&&) = delete;
	constexpr ~component_pool() noexcept override {
		if constexpr (global<T>) {
			std::destroy_n(head->data, head->range.ucount());
			alloc.deallocate(head->data, head->range.count());
			std::destroy_at(head);
			alloc_chunk.deallocate(head, 1);
		} else {
			free_all_chunks();
		}
	}

	// Add a span of component to a range of entities
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	constexpr void add_span(entity_range const range, std::span<const T> span) noexcept requires(!detail::unbound<T>) {
		Expects(range.count() == std::ssize(span));

		// Add the range and function to a temp storage
		deferred_spans.local().emplace_back(range, span);
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	constexpr void add(entity_range const range, T&& component) noexcept {
		if constexpr (tagged<T>) {
			deferred_adds.local().push_back(range);
		} else {
			deferred_adds.local().emplace_back(range, std::forward<T>(component));
		}
	}

	// Add a component to a range of entity.
	// Pre: entities has not already been added, or is in queue to be added
	//      This condition will not be checked until 'process_changes' is called.
	constexpr void add(entity_range const range, T const& component) noexcept {
		if constexpr (tagged<T>) {
			deferred_adds.local().push_back(range);
		} else {
			deferred_adds.local().emplace_back(range, component);
		}
	}

	// Return the shared component
	constexpr T& get_shared_component() noexcept requires global<T> {
		return head->data[0];
	}

	// Remove an entity from the component pool.
	constexpr void remove(entity_id const id) noexcept {
		remove({id, id});
	}

	// Remove an entity from the component pool.
	constexpr void remove(entity_range const range) noexcept {
		deferred_removes.local().push_back(range);
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	constexpr T* find_component_data(entity_id const id) noexcept requires(!global<T>) {
		return const_cast<T*>(std::as_const(*this).find_component_data(id));
	}

	// Returns an entities component.
	// Returns nullptr if the entity is not found in this pool
	constexpr T const* find_component_data(entity_id const id) const noexcept requires(!global<T>) {
		if (head == nullptr)
			return nullptr;

		auto const range_it = find_in_ordered_active_ranges({id, id});
		if (range_it != ordered_active_ranges.end() && range_it->contains(id)) {
			auto const chunk_it = ordered_chunks.begin() + ranges_dist(range_it);
			chunk* c = (*chunk_it);
			auto const offset = c->range.offset(id);
			return &c->data[offset];
		}

		return nullptr;
	}

	// Merge all the components queued for addition to the main storage,
	// and remove components queued for removal
	constexpr void process_changes() noexcept override {
		process_remove_components();
		process_add_components();
	}

	// Returns the number of active entities in the pool
	constexpr size_t num_entities() const noexcept {
		size_t count = 0;

		for (entity_range const r : ordered_active_ranges) {
			count += r.ucount();
		}

		return count;
	}

	// Returns the number of active components in the pool
	constexpr size_t num_components() const noexcept {
		if constexpr (unbound<T>)
			return 1;
		else
			return num_entities();
	}

	// Returns the number of chunks in use
	constexpr size_t num_chunks() const noexcept {
		return ordered_chunks.size();
	}

	constexpr chunk const* get_head_chunk() const noexcept {
		return head;
	}

	// Clears the pools state flags
	constexpr void clear_flags() noexcept override {
		components_added = false;
		components_removed = false;
		components_modified = false;
	}

	// Returns true if components has been added since last clear_flags() call
	constexpr bool has_more_components() const noexcept {
		return components_added;
	}

	// Returns true if components has been removed since last clear_flags() call
	constexpr bool has_less_components() const noexcept {
		return components_removed;
	}

	// Returns true if components has been added/removed since last clear_flags() call
	constexpr bool has_component_count_changed() const noexcept {
		return components_added || components_removed;
	}

	constexpr bool has_components_been_modified() const noexcept {
		return has_component_count_changed() || components_modified;
	}

	// Returns the pools entities
	constexpr entity_range_view get_entities() const noexcept {
		if constexpr (detail::global<T>) {
			// globals are accessible to all entities
			//static constinit entity_range global_range = entity_range::all();
			//return entity_range_view{&global_range, 1};
			return ordered_active_ranges;
		} else {
			return ordered_active_ranges;
		}
	}

	// Returns true if an entity has a component in this pool
	constexpr bool has_entity(entity_id const id) const noexcept {
		return has_entity({id, id});
	}

	// Returns true if an entity range has components in this pool
	constexpr bool has_entity(entity_range const& range) const noexcept {
		auto const it = find_in_ordered_active_ranges(range);

		if (it == ordered_active_ranges.end())
			return false;

		return it->contains(range);
	}

	// Clear all entities from the pool
	constexpr void clear() noexcept override {
		// Remember if components was removed from the pool
		bool const is_removed = (nullptr != head);

		// Clear all data
		free_all_chunks();
		deferred_adds.reset();
		deferred_spans.reset();
		deferred_removes.reset();
		ordered_active_ranges.clear();
		ordered_chunks.clear();
		clear_flags();

		// Save the removal state
		components_removed = is_removed;
	}

	// Flag that components has been modified
	constexpr void notify_components_modified() noexcept {
		components_modified = true;
	}

private:
	constexpr chunk* create_new_chunk(entity_range const range, entity_range const active, T* data = nullptr, chunk* next = nullptr,
							bool owns_data = true, bool split_data = false) noexcept {
		chunk* c = alloc_chunk.allocate(1);
		std::construct_at(c, range, active, data, next, owns_data , split_data);

		auto const range_it = find_in_ordered_active_ranges(active);
		auto const dist = ranges_dist(range_it);
		ordered_active_ranges.insert(range_it, active);

		auto const chunk_it = ordered_chunks.begin() + dist;
		ordered_chunks.insert(chunk_it, c);

		return c;
	}

	constexpr chunk* create_new_chunk(std::forward_iterator auto iter) noexcept {
		entity_range const r = std::get<0>(*iter);
		chunk* c = create_new_chunk(r, r);
		if constexpr (!unbound<T>) {
			c->data = alloc.allocate(r.ucount());
			construct_range_in_chunk(c, r, std::get<1>(*iter));
		}

		return c;
	}

	constexpr void free_chunk(chunk* c) noexcept {
		remove_range_to_chunk(c->active);

		if (c->owns_data) {
			if (c->has_split_data && nullptr != c->next) {
				// transfer ownership
				c->next->owns_data = true;
			} else {
				if constexpr (!unbound<T>) {
					std::destroy_n(c->data, c->active.ucount());
					alloc.deallocate(c->data, c->range.count());
				}
			}
		}

		std::destroy_at(c);
		alloc_chunk.deallocate(c, 1);
	}

	constexpr void free_all_chunks() noexcept {
		ordered_active_ranges.clear();
		ordered_chunks.clear();
		chunk* curr = head;
		while (curr != nullptr) {
			chunk* next = curr->next;
			free_chunk(curr);
			curr = next;
		}
		head = nullptr;
		set_data_removed();
	}

	constexpr auto find_in_ordered_active_ranges(entity_range const rng) noexcept {
		return std::ranges::lower_bound(ordered_active_ranges, rng, std::less{});
	}
	constexpr auto find_in_ordered_active_ranges(entity_range const rng) const noexcept {
		return std::ranges::lower_bound(ordered_active_ranges, rng, std::less{});
	}

	constexpr ptrdiff_t ranges_dist(std::vector<entity_range>::const_iterator it) const noexcept {
		return std::distance(ordered_active_ranges.begin(), it);
	}

	// Removes a range and chunk from the map
	constexpr void remove_range_to_chunk(entity_range const rng) noexcept {
		auto const it = find_in_ordered_active_ranges(rng);
		if (it != ordered_active_ranges.end() && *it == rng) {
			auto const dist = ranges_dist(it);

			ordered_active_ranges.erase(it);
			ordered_chunks.erase(ordered_chunks.begin() + dist);
		}
	}

	// Updates a key in the range-to-chunk map
	constexpr void update_range_to_chunk_key(entity_range const old, entity_range const update) noexcept {
		auto it = find_in_ordered_active_ranges(old);
		*it = update;
	}

	// Flag that components has been added
	constexpr void set_data_added() noexcept {
		components_added = true;
	}

	// Flag that components has been removed
	constexpr void set_data_removed() noexcept {
		components_removed = true;
	}

	// Verify the 'add*' functions precondition.
	// An entity can not have more than one of the same component
	constexpr bool has_duplicate_entities() const noexcept {
		if (!ordered_active_ranges.empty()) {
			for (size_t i = 0; i < ordered_active_ranges.size() - 1; ++i) {
				if (ordered_active_ranges[i].overlaps(ordered_active_ranges[i + 1]))
					return true;
			}
		}

		return false;
	};

	constexpr static bool is_equal(T const& lhs, T const& rhs) noexcept requires std::equality_comparable<T> {
		return lhs == rhs;
	}
	constexpr static bool is_equal(T const& /*lhs*/, T const& /*rhs*/) noexcept requires tagged<T> {
		// Tags are empty, so always return true
		return true;
	}
	constexpr static bool is_equal(T const&, T const&) noexcept {
		// Type can not be compared, so always return false.
		// memcmp is a no-go because it also compares padding in types,
		// and it is not constexpr
		return false;
	}

	template <typename Data>
	constexpr void construct_range_in_chunk(chunk* c, entity_range range, Data const& comp_data) noexcept requires(!unbound<T>) {
		Expects(c != nullptr);

		// Offset into the chunks data
		auto const ent_offset = c->range.offset(range.first());

		for (entity_offset i = 0; i < range.ucount(); ++i) {
			// Construct from a value or a a span of values
			if constexpr (std::is_same_v<T, Data>) {
				std::construct_at(&c->data[ent_offset + i], comp_data);
			} else {
				std::construct_at(&c->data[ent_offset + i], comp_data[i]);
			}
		}
	}

	constexpr void fill_data_in_existing_chunk(chunk*& curr, chunk*& prev, entity_range r) noexcept {
		// If split chunks are encountered, skip forward to the chunk closest to r
		if (curr->has_split_data) {
			while (nullptr != curr->next && curr->next->range.contains(r) && curr->next->active < r) {
				prev = curr;
				curr = curr->next;
			}
		}

		if (curr->active.adjacent(r)) {
			// The two ranges are next to each other, so add the data to existing chunk
			entity_range active_range = entity_range::merge(curr->active, r);
			update_range_to_chunk_key(curr->active, active_range);
			curr->active = active_range;

			chunk* next = curr->next;

			// Check to see if this chunk can be collapsed into 'prev'
			if (nullptr != prev) {
				if (prev->active.adjacent(curr->active)) {
					active_range = entity_range::merge(prev->active, curr->active);
					remove_range_to_chunk(prev->active);
					update_range_to_chunk_key(prev->active, active_range);
					prev->active = active_range;

					free_chunk(curr);
					prev->next = next;
					curr = next;
					if (next != nullptr)
						next = next->next;
				}
			}

			// Check to see if 'next' can be collapsed into this chunk
			if (nullptr != next) {
				if (curr->active.adjacent(next->active)) {
					active_range = entity_range::merge(curr->active, next->active);
					remove_range_to_chunk(next->active);
					update_range_to_chunk_key(curr->active, active_range);

					curr->active = active_range;
					curr->next = next->next;

					// split_data is true if the next chunk is also in the current range
					curr->has_split_data = (curr->next != nullptr) && (curr->range == curr->next->range);

					free_chunk(next);
				}
			}
		} else {
			// There is a gap between the two ranges, so split the chunk
			if (r < curr->active) {
				bool const is_head_chunk = (head == curr);
				bool const curr_owns_data = curr->owns_data;
				curr->owns_data = false;
				curr = create_new_chunk(curr->range, r, curr->data, curr, curr_owns_data, true);

				// Update head pointer
				if (is_head_chunk)
					head = curr;

				// Make the previous chunk point to curr
				if (prev != nullptr)
					prev->next = curr;
			} else {
				curr->has_split_data = true;
				curr->next = create_new_chunk(curr->range, r, curr->data, curr->next, false, false);
			}
		}
	}

	// Try to combine two ranges. With data
	constexpr static bool combiner_bound(entity_data& a, entity_data const& b) requires(!unbound<T>) {
		auto& [a_rng, a_data] = a;
		auto const& [b_rng, b_data] = b;

		if (a_rng.adjacent(b_rng) && is_equal(a_data, b_data)) {
			a_rng = entity_range::merge(a_rng, b_rng);
			return true;
		} else {
			return false;
		}
	}

	// Try to combine two ranges. Without data
	constexpr static bool combiner_unbound(entity_data& a, entity_data const& b) requires(unbound<T>) {
		auto& [a_rng] = a;
		auto const& [b_rng] = b;

		if (a_rng.adjacent(b_rng)) {
			a_rng = entity_range::merge(a_rng, b_rng);
			return true;
		} else {
			return false;
		}
	}

	// Add new queued entities and components to the main storage.
	constexpr void process_add_components() noexcept {
		// Combine the components in to a single vector
		std::vector<entity_data> adds;
		std::vector<entity_span> spans;
		deferred_adds.gather_flattened(std::back_inserter(adds));
		deferred_spans.gather_flattened(std::back_inserter(spans));

		if (adds.empty() && spans.empty()) {
			return;
		}

		// Clear the current adds
		deferred_adds.reset();
		deferred_spans.reset();

		// Sort the input
		auto const comparator = [](auto const& l, auto const& r) {
			return std::get<0>(l) < std::get<0>(r);
		};
		if (!std::is_constant_evaluated() || (sizeof(entity_data) * adds.size() < parallelization_size_tipping_point))
			std::sort(adds.begin(), adds.end(), comparator);
		else
			std::sort(std::execution::par, adds.begin(), adds.end(), comparator);

		if (!std::is_constant_evaluated() || (sizeof(entity_data) * spans.size() < parallelization_size_tipping_point))
			std::sort(spans.begin(), spans.end(), comparator);
		else
			std::sort(std::execution::par, spans.begin(), spans.end(), comparator);

		// Merge adjacent ranges that has the same data
		if constexpr (unbound<T>)
			combine_erase(adds, combiner_unbound);
		else
			combine_erase(adds, combiner_bound);

		// Do the insertions
		chunk* prev = nullptr;
		chunk* curr = head;

		auto it_adds = adds.begin();
		auto it_spans = spans.begin();

		// Create head chunk if needed
		if (head == nullptr) {
			if (it_adds != adds.end()) {
				head = create_new_chunk(it_adds);
				++it_adds;
			} else {
				head = create_new_chunk(it_spans);
				++it_spans;
			}

			curr = head;
		}

		auto const merge_data = [&](std::forward_iterator auto const& iter) {
			if (curr == nullptr) {
				auto new_chunk = create_new_chunk(iter);
				new_chunk->next = curr;
				curr = new_chunk;
				prev->next = curr;
			} else {
				entity_range const r = std::get<0>(*iter);

				// Move current chunk pointer forward
				while (nullptr != curr->next && curr->next->range.contains(r) && curr->next->active < r) {
					prev = curr;
					curr = curr->next;
				}

				if (curr->range.overlaps(r)) {
					// Incoming range overlaps the current one, so add it into 'curr'
					fill_data_in_existing_chunk(curr, prev, r);
					if constexpr (!unbound<T>) {
						construct_range_in_chunk(curr, r, std::get<1>(*iter));
					}
				} else if (curr->range < r) {
					// Incoming range is larger than the current one, so add it after 'curr'
					auto new_chunk = create_new_chunk(iter);
					new_chunk->next = curr->next;
					curr->next = new_chunk;

					prev = curr;
					curr = curr->next;

				} else if (r < curr->range) {
					// Incoming range is less than the current one, so add it before 'curr' (after 'prev')
					auto new_chunk = create_new_chunk(iter);
					new_chunk->next = curr;
					if (head == curr)
						head = new_chunk;
					curr = new_chunk;
					if (prev != nullptr)
						prev->next = curr;
				}
			}
		};

		// Fill in values
		while (it_adds != adds.end()) {
			merge_data(it_adds);
			++it_adds;
		}

		// Fill in spans
		prev = nullptr;
		curr = head;
		while (it_spans != spans.end()) {
			merge_data(it_spans);
			++it_spans;
		}

		// Check it
		Expects(false == has_duplicate_entities());

		// Update the state
		set_data_added();
	}

	// Removes the entities and components
	constexpr void process_remove_components() noexcept {
		// Collect all the ranges to remove
		std::vector<entity_range> vec;
		deferred_removes.gather_flattened(std::back_inserter(vec));

		// Dip if there is nothing to do
		if (vec.empty() || nullptr == head) {
			return;
		}

		// Sort the ranges to remove
		if (!std::is_constant_evaluated() || (sizeof(entity_range) * vec.size() < parallelization_size_tipping_point))
			std::sort(vec.begin(), vec.end());
		else
			std::sort(std::execution::par, vec.begin(), vec.end());

		// Remove ranges
		process_remove_components(vec);

		// Update the state
		set_data_removed();
	}

	constexpr void process_remove_components(std::vector<entity_range>& removes) noexcept {
		chunk* prev = nullptr;
		chunk* it_chunk = head;
		auto it_rem = removes.begin();

		while (it_chunk != nullptr && it_rem != removes.end()) {
			if (it_chunk->active < *it_rem) {
				prev = it_chunk;
				it_chunk = it_chunk->next;
			} else if (*it_rem < it_chunk->active) {
				++it_rem;
			} else {
				if (it_chunk->active == *it_rem) {
					// remove an entire range
					// todo: move to a free-store?
					chunk* next = it_chunk->next;

					// Update head pointer
					if (it_chunk == head) {
						head = next;
					}

					// Delete the chunk and potentially its data
					free_chunk(it_chunk);

					// Update the previous chunks next pointer
					if (nullptr != prev)
						prev->next = next;

					it_chunk = next;
				} else {
					// remove partial range
					auto const [left_range, maybe_split_range] = entity_range::remove(it_chunk->active, *it_rem);

					// Update the active range
					update_range_to_chunk_key(it_chunk->active, left_range);
					it_chunk->active = left_range;

					// Destroy the removed components
					if constexpr (!unbound<T>) {
						auto const offset = it_chunk->range.offset(it_rem->first());
						std::destroy_n(&it_chunk->data[offset], it_rem->ucount());
					}

					if (maybe_split_range.has_value()) {
						// If two ranges were returned, split this chunk
						it_chunk->has_split_data = true;
						it_chunk->next =
							create_new_chunk(it_chunk->range, maybe_split_range.value(), it_chunk->data, it_chunk->next, false);
					}

					prev = it_chunk;
					it_chunk = it_chunk->next;
				}
			}
		}
	}

	// Removes transient components
	constexpr void process_remove_components() noexcept requires transient<T> {
		// All transient components are removed each cycle
		free_all_chunks();
	}
};
} // namespace ecs::detail

#endif // !ECS_DETAIL_COMPONENT_POOL_H
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
#ifndef ECS_SYSTEM_DEFS_H_
#define ECS_SYSTEM_DEFS_H_

// Contains definitions that are used by the systems classes

namespace ecs::detail {
template <class T>
constexpr static bool is_entity = std::is_same_v<std::remove_cvref_t<T>, entity_id>;

// If given a parent, convert to detail::parent_id, otherwise do nothing
template <typename T>
using reduce_parent_t =
	std::conditional_t<std::is_pointer_v<T>, std::conditional_t<is_parent<std::remove_pointer_t<T>>::value, parent_id*, T>,
					   std::conditional_t<is_parent<T>::value, parent_id, T>>;

// Alias for stored pools
template <class T>
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

template <template <class...> class Parent, class... ParentComponents> // partial specialization
struct parent_type_list<Parent<ParentComponents...>> {
	static_assert(!(is_parent<ParentComponents>::value || ...), "parents in parents not supported");
	using type = type_list<ParentComponents...>;
};
template <typename T>
using parent_type_list_t = typename parent_type_list<T>::type;

// Helper to extract the parent pool types
template <typename T>
struct parent_pool_detect; // primary template

template <template <class...> class Parent, class... ParentComponents> // partial specialization
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
	return *std::get<pool<T>>(pools);
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

	// Filter: return a nullptr
	if constexpr (std::is_pointer_v<T>) {
		static_cast<void>(entity);
		return nullptr;

		// Tag: return a pointer to some dummy storage
	} else if constexpr (tagged<T>) {
		// TODO thread_local. static syncs threads
		thread_local char dummy_arr[sizeof(T)];
		return reinterpret_cast<T*>(dummy_arr);

		// Global: return the shared component
	} else if constexpr (global<T>) {
		return &get_pool<T>(pools).get_shared_component();

		// Parent component: return the parent with the types filled out
	} else if constexpr (std::is_same_v<reduce_parent_t<T>, parent_id>) {
		using parent_type = std::remove_cvref_t<Component>;
		parent_id pid = *get_pool<parent_id>(pools).find_component_data(entity);

		parent_type_list_t<parent_type> pt;
		auto const tup_parent_ptrs = apply(
			[&](auto*... parent_types) {
				return std::make_tuple(get_entity_data<std::remove_pointer_t<decltype(parent_types)>>(pid, pools)...);
			},
			pt);

		return parent_type{pid, tup_parent_ptrs};

		// Standard: return the component from the pool
	} else {
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

// The type of a single component argument
template <typename Component>
using component_argument = std::conditional_t<is_parent<std::remove_cvref_t<Component>>::value,
											  std::remove_cvref_t<Component>,	// parent components are stored as copies
											  std::remove_cvref_t<Component>*>; // rest are pointers

// Holds a pointer to the first component from each pool
template <class FirstComponent, class... Components>
using argument_tuple = std::conditional_t<is_entity<FirstComponent>, std::tuple<component_argument<Components>...>,
										  std::tuple<component_argument<FirstComponent>, component_argument<Components>...>>;

// Holds a single entity id and its arguments
template <class FirstComponent, class... Components>
using single_argument = decltype(std::tuple_cat(std::tuple<entity_id>{0}, std::declval<argument_tuple<FirstComponent, Components...>>()));

// Holds an entity range and its arguments
template <class FirstComponent, class... Components>
using range_argument =
	decltype(std::tuple_cat(std::tuple<entity_range>{{0, 1}}, std::declval<argument_tuple<FirstComponent, Components...>>()));

// Tuple holding component pools
template <class FirstComponent, class... Components>
using tup_pools = std::conditional_t<is_entity<FirstComponent>, std::tuple<pool<reduce_parent_t<Components>>...>,
									 std::tuple<pool<reduce_parent_t<FirstComponent>>, pool<reduce_parent_t<Components>>...>>;

} // namespace ecs::detail

#endif // !ECS_SYSTEM_DEFS_H_
#ifndef POOL_ENTITY_WALKER_H_
#define POOL_ENTITY_WALKER_H_


namespace ecs::detail {

// The type of a single component argument
template <typename Component>
using walker_argument = reduce_parent_t<std::remove_cvref_t<Component>>*;

template <typename T>
struct pool_type_detect; // primary template

template <template <class> class Pool, class Type>
struct pool_type_detect<Pool<Type>> {
	using type = Type;
};
template <template <class> class Pool, class Type>
struct pool_type_detect<Pool<Type>*> {
	using type = Type;
};

template <typename T>
struct tuple_pool_type_detect; // primary template

template <template <class...> class Tuple, class... PoolTypes> // partial specialization
struct tuple_pool_type_detect<const Tuple<PoolTypes* const...>> {
	using type = std::tuple<typename pool_type_detect<PoolTypes>::type*...>;
};

template <typename T>
using tuple_pool_type_detect_t = typename tuple_pool_type_detect<T>::type;

// Linearly walks one-or-more component pools
// TODO why is this not called an iterator?
template <class Pools>
struct pool_entity_walker {
	void reset(Pools* _pools, entity_range_view view) {
		pools = _pools;
		ranges.assign(view.begin(), view.end());
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
		using T = std::remove_cvref_t<Component>;

		if constexpr (std::is_pointer_v<T>) {
			// Filter: return a nullptr
			return nullptr;

		} else if constexpr (tagged<T>) {
			// Tag: return a pointer to some dummy storage
			thread_local char dummy_arr[sizeof(T)];
			return reinterpret_cast<T*>(dummy_arr);

		} else if constexpr (global<T>) {
			// Global: return the shared component
			return &get_pool<T>(*pools).get_shared_component();

		} else if constexpr (std::is_same_v<reduce_parent_t<T>, parent_id>) {
			// Parent component: return the parent with the types filled out
			using parent_type = std::remove_cvref_t<Component>;
			parent_id* pid = get_pool<parent_id>(*pools).find_component_data(ranges_it->first() + offset);

			auto const tup_parent_ptrs = apply_type<parent_type_list_t<parent_type>>([&]<typename... ParentType>() {
				return std::make_tuple(get_entity_data<ParentType>(*pid, *pools)...);
			});

			return parent_type{*pid, tup_parent_ptrs};
		} else {
			// Standard: return the component from the pool
			return get_pool<T>(*pools).find_component_data(ranges_it->first() + offset);
		}
	}

private:
	//void update_pool_offsets() {
	//	if (done())
	//		return;

	//	std::apply(
	//		[this](auto* const... in_pools) {
	//			auto const f = [&](auto pool) {
	//				using pool_inner_type = typename pool_type_detect<decltype(pool)>::type;
	//				auto const component_ptr = pool->find_component_data(ranges_it->first());
	//				std::get<pool_inner_type*>(pointers) = component_ptr;
	//			};

	//			(f(in_pools), ...);
	//		},
	//		*pools);
	//}

private:
	// The ranges to iterate over
	std::vector<entity_range> ranges;

	// Iterator over the current range
	std::vector<entity_range>::iterator ranges_it;

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
template <class Pools>
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
		using T = std::remove_cvref_t<Component>;

		entity_id const entity = it->first();

		// Filter: return a nullptr
		if constexpr (std::is_pointer_v<T>) {
			static_cast<void>(entity);
			return nullptr;

			// Tag: return a pointer to some dummy storage
		} else if constexpr (tagged<T>) {
			static char dummy_arr[sizeof(T)];
			return reinterpret_cast<T*>(dummy_arr);

			// Global: return the shared component
		} else if constexpr (global<T>) {
			return &get_pool<T>(pools).get_shared_component();

			// Parent component: return the parent with the types filled out
		} else if constexpr (std::is_same_v<reduce_parent_t<T>, parent_id>) {
			using parent_type = std::remove_cvref_t<Component>;
			parent_id pid = *get_pool<parent_id>(pools).find_component_data(entity);

			parent_type_list_t<parent_type> pt;
			auto const tup_parent_ptrs = apply(
				[&](auto*... parent_types) {
					return std::make_tuple(get_entity_data<std::remove_pointer_t<decltype(parent_types)>>(pid, pools)...);
				},
				pt);

			return parent_type{pid, tup_parent_ptrs};

			// Standard: return the component from the pool
		} else {
			return get_pool<T>(pools).find_component_data(entity);
		}
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
struct get_type {
	using type = T;
};

template <std::invocable<entity_type> T>
struct get_type<T> {
	using type = std::invoke_result_t<T, entity_type>;
};

template <typename T>
using get_type_t = typename get_type<T>::type;

// Returns true if all types passed are unique
template <typename First, typename... T>
constexpr bool unique_types() {
	if constexpr ((std::is_same_v<First, T> || ...))
		return false;
	else {
		if constexpr (sizeof...(T) == 0)
			return true;
		else
			return unique_types<T...>();
	}
}

template <typename First, typename... T>
constexpr static bool unique_types_v = unique_types<get_type<First>, get_type_t<T>...>();

// Ensure that any type in the parameter pack T is only present once.
template <typename First, typename... T>
concept unique = unique_types_v<First, T...>;

// Gets the type a sorting function operates on.
template <class R, class A, class B, class... C>
struct get_sorter_types_impl {
	explicit get_sorter_types_impl(R (*)(A, B)) {
		static_assert(sizeof...(C) == 0, "two arguments expected in sorting predicate");
		static_assert(std::is_same_v<A, B>, "types must be identical");
	}
	explicit get_sorter_types_impl(R (A::*)(B, C...) const) {
		static_assert(sizeof...(C) == 1, "two arguments expected in sorting predicate");
		static_assert(std::is_same_v<B, C...>, "types must be identical");
	}

	using type1 = std::conditional_t<sizeof...(C) == 0, std::remove_cvref_t<A>, std::remove_cvref_t<B>>;
	using type2 = std::conditional_t<sizeof...(C) == 0, std::remove_cvref_t<B>, std::remove_cvref_t<type_list_at<0, type_list<C...>>>>;
};

template <class T1, class T2>
struct get_sorter_types {
	template <class T>
	requires requires {
		&T::operator();
	}
	explicit get_sorter_types(T) {}

	template <class R, class A, class B>
	explicit get_sorter_types(R (*)(A, B)) {}

	using type1 = std::remove_cvref_t<T1>;
	using type2 = std::remove_cvref_t<T2>;
};

template <class R, class A, class B>
get_sorter_types(R (*)(A, B)) -> get_sorter_types<A, B>;

template <class T>
get_sorter_types(T) -> get_sorter_types< // get_sorter_types(&T::operator()) // deduction guides can't refer to other guides :/
	typename decltype(get_sorter_types_impl(&T::operator()))::type1, typename decltype(get_sorter_types_impl(&T::operator()))::type2>;

// Implement the requirements for ecs::parent components
template <typename C>
constexpr void verify_parent_component() {
	if constexpr (detail::is_parent<std::remove_cvref_t<C>>::value) {
		using parent_subtypes = parent_type_list_t<std::remove_cvref_t<C>>;
		constexpr size_t total_subtypes = type_list_size<parent_subtypes>;

		if constexpr (total_subtypes > 0) {
			// Count all the filters in the parent type
			constexpr size_t num_subtype_filters =
				apply_type<parent_subtypes>([]<typename... Types>() { return (std::is_pointer_v<Types> + ...); });

			// Count all the types minus filters in the parent type
			constexpr size_t num_parent_subtypes = total_subtypes - num_subtype_filters;
			// std::tuple_size_v<parent_types_tuple_t<std::remove_cvref_t<C>>> - num_parent_subtype_filters;

			// If there is one-or-more sub-components,
			// then the parent must be passed as a reference
			if constexpr (num_parent_subtypes > 0) {
				static_assert(std::is_reference_v<C>, "parents with non-filter sub-components must be passed as references");
			}
		}
	}
}

// Implement the requirements for tagged components
template <typename C>
constexpr void verify_tagged_component() {
	if constexpr (detail::tagged<C>)
		static_assert(!std::is_reference_v<C> && (sizeof(C) == 1), "components flagged as 'tag' must not be references");
}

// Implement the requirements for global components
template <typename C>
constexpr void verify_global_component() {
	if constexpr (detail::global<C>)
		static_assert(!detail::tagged<C> && !detail::transient<C>, "components flagged as 'global' must not be 'tag's or 'transient'");
}

// Implement the requirements for immutable components
template <typename C>
constexpr void verify_immutable_component() {
	if constexpr (detail::immutable<C>)
		static_assert(std::is_const_v<std::remove_reference_t<C>>, "components flagged as 'immutable' must also be const");
}

template <class R, class FirstArg, class... Args>
constexpr void system_verifier() {
	static_assert(std::is_same_v<R, void>, "systems can not have returnvalues");

	static_assert(unique_types_v<FirstArg, Args...>, "component parameter types can only be specified once");

	if constexpr (is_entity<FirstArg>) {
		static_assert(sizeof...(Args) > 0, "systems must take at least one component argument");

		// Make sure the first entity is not passed as a reference
		static_assert(!std::is_reference_v<FirstArg>, "ecs::entity_id must not be passed as a reference");
	}

	verify_immutable_component<FirstArg>();
	(verify_immutable_component<Args>(), ...);

	verify_global_component<FirstArg>();
	(verify_global_component<Args>(), ...);

	verify_tagged_component<FirstArg>();
	(verify_tagged_component<Args>(), ...);

	verify_parent_component<FirstArg>();
	(verify_parent_component<Args>(), ...);
}

// A small bridge to allow the Lambda to activate the system verifier
template <class R, class C, class FirstArg, class... Args>
struct system_to_lambda_bridge {
	explicit system_to_lambda_bridge(R (C::*)(FirstArg, Args...)) {
		system_verifier<R, FirstArg, Args...>();
	};
	explicit system_to_lambda_bridge(R (C::*)(FirstArg, Args...) const) {
		system_verifier<R, FirstArg, Args...>();
	};
	explicit system_to_lambda_bridge(R (C::*)(FirstArg, Args...) noexcept) {
		system_verifier<R, FirstArg, Args...>();
	};
	explicit system_to_lambda_bridge(R (C::*)(FirstArg, Args...) const noexcept) {
		system_verifier<R, FirstArg, Args...>();
	};
};

// A small bridge to allow the function to activate the system verifier
template <class R, class FirstArg, class... Args>
struct system_to_func_bridge {
	explicit system_to_func_bridge(R (*)(FirstArg, Args...)) {
		system_verifier<R, FirstArg, Args...>();
	};
	explicit system_to_func_bridge(R (*)(FirstArg, Args...) noexcept) {
		system_verifier<R, FirstArg, Args...>();
	};
};

template <typename T>
concept type_is_lambda = requires {
	&T::operator();
};

template <typename T>
concept type_is_function = requires(T t) {
	system_to_func_bridge{t};
};

template <typename TupleOptions, typename SystemFunc, typename SortFunc>
void make_system_parameter_verifier() {
	bool constexpr is_lambda = type_is_lambda<SystemFunc>;
	bool constexpr is_func = type_is_function<SystemFunc>;

	static_assert(is_lambda || is_func, "Systems can only be created from lambdas or free-standing functions");

	// verify the system function
	if constexpr (is_lambda) {
		system_to_lambda_bridge const stlb(&SystemFunc::operator());
	} else if constexpr (is_func) {
		system_to_func_bridge const stfb(SystemFunc{});
	}

	// verify the sort function
	if constexpr (!std::is_same_v<std::nullptr_t, SortFunc>) {
		bool constexpr is_sort_lambda = type_is_lambda<SystemFunc>;
		bool constexpr is_sort_func = type_is_function<SystemFunc>;

		static_assert(is_sort_lambda || is_sort_func, "invalid sorting function");

		using sort_types = decltype(get_sorter_types(SortFunc{}));
		if constexpr (is_sort_lambda) {
			static_assert(std::predicate<SortFunc, typename sort_types::type1, typename sort_types::type2>,
						  "Sorting function is not a predicate");
		} else if constexpr (is_sort_func) {
			static_assert(std::predicate<SortFunc, typename sort_types::type1, typename sort_types::type2>,
						  "Sorting function is not a predicate");
		}
	}
}

} // namespace ecs::detail

#endif // !ECS_VERIFICATION_H
#ifndef ECS_DETAIL_ENTITY_RANGE
#define ECS_DETAIL_ENTITY_RANGE


namespace ecs::detail {
// Find the intersectsions between two sets of ranges
inline std::vector<entity_range> intersect_ranges(entity_range_view view_a, entity_range_view view_b) {
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
}

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
			merge_or_add(result, range_a);
			if (++it_a != view_a.end())
				range_a = *it_a;
		} else if (it_b->contains(range_a)) {
			// Range 'a' is contained entirely in range 'b',
			// which means that 'a' will not be added.
			if (++it_a != view_a.end())
				range_a = *it_a;
		} else if (range_a < *it_b) {
			// The whole 'a' range is before 'b', so add range 'a'
			merge_or_add(result, range_a);

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
				merge_or_add(result, res.first);
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
					merge_or_add(result, res.first);

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

template <class Component, typename TuplePools>
void pool_intersect(std::vector<entity_range>& ranges, TuplePools const& pools) {
	using T = std::remove_cvref_t<Component>;

	// Skip globals and parents
	if constexpr (detail::global<T>) {
		// do nothing
	} else if constexpr (detail::is_parent<T>::value) {
		ranges = intersect_ranges(ranges, get_pool<parent_id>(pools).get_entities());
	} else if constexpr (std::is_pointer_v<T>) {
		// do nothing
	} else {
		ranges = intersect_ranges(ranges, get_pool<T>(pools).get_entities());
	}
}

template <class Component, typename TuplePools>
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
template <class FirstComponent, class... Components, typename TuplePools>
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
template <class Options, class UpdateFn, class TupPools, class FirstComponent, class... Components>
class system : public system_base {
	virtual void do_run() = 0;
	virtual void do_build(entity_range_view) = 0;

public:
	system(UpdateFn func, TupPools tup_pools) : update_func{func}, pools{tup_pools}, pool_parent_id{nullptr} {
		if constexpr (has_parent_types) {
			pool_parent_id = &detail::get_pool<parent_id>(pools);
		}
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
		if constexpr (!is_entity<FirstComponent>) {
			notify_pool_modifed<FirstComponent>();
		}
		(notify_pool_modifed<Components>(), ...);
	}

	template <typename T>
	void notify_pool_modifed() {
		if constexpr (detail::is_parent<T>::value && !is_read_only<T>()) { // writeable parent
			// Recurse into the parent types
			for_each_type<parent_type_list_t<T>>([this]<typename... ParentTypes>() { (this->notify_pool_modifed<ParentTypes>(), ...); });
		} else if constexpr (std::is_reference_v<T> && !is_read_only<T>() && !std::is_pointer_v<T>) {
			get_pool<reduce_parent_t<std::remove_cvref_t<T>>>(pools).notify_components_modified();
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
		auto const check_hash = [hash]<typename T>() { return get_type_hash<T>() == hash; };

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
		auto const check_writes = [hash]<typename T>() { return get_type_hash<std::remove_cvref_t<T>>() == hash && !is_read_only<T>(); };

		if (any_of_type<component_list>(check_writes))
			return true;

		if constexpr (has_parent_types) {
			return any_of_type<parent_component_list>(check_writes);
		} else {
			return false;
		}
	}

private:
	// Handle changes when the component pools change
	void process_changes(bool force_rebuild) override {
		if (force_rebuild) {
			find_entities();
			return;
		}

		if (!is_enabled()) {
			return;
		}

		bool const modified = std::apply([](auto... p) { return (p->has_component_count_changed() || ...); }, pools);

		if (modified) {
			find_entities();
		}
	}

	// Locate all the entities affected by this system
	// and send them to the argument builder
	void find_entities() {
		if constexpr (num_components == 1 && !has_parent_types) {
			// Build the arguments
			entity_range_view const entities = std::get<0>(pools)->get_entities();
			do_build(entities);
		} else {
			// When there are more than one component required for a system,
			// find the intersection of the sets of entities that have those components

			std::vector<entity_range> ranges = find_entity_pool_intersections<FirstComponent, Components...>(pools);

			if constexpr (has_parent_types) {
				// the vector of ranges to remove
				std::vector<entity_range> ents_to_remove;

				for (auto const& range : ranges) {
					for (auto const ent : range) {
						// Get the parent ids in the range
						parent_id const pid = *pool_parent_id->find_component_data(ent);

						// Does tests on the parent sub-components to see they satisfy the constraints
						// ie. a 'parent<int*, float>' will return false if the parent does not have a float or
						// has an int.
						for_each_type<parent_component_list>([&pid, this, ent, &ents_to_remove]<typename T>() {
							// Get the pool of the parent sub-component
							auto const& sub_pool = detail::get_pool<T>(this->pools);

							if constexpr (std::is_pointer_v<T>) {
								// The type is a filter, so the parent is _not_ allowed to have this component
								if (sub_pool.has_entity(pid)) {
									merge_or_add(ents_to_remove, entity_range{ent, ent});
								}
							} else {
								// The parent must have this component
								if (!sub_pool.has_entity(pid)) {
									merge_or_add(ents_to_remove, entity_range{ent, ent});
								}
							}
						});
					}
				}

				// Remove entities from the result
				ranges = difference_ranges(ranges, ents_to_remove);
			}

			do_build(ranges);
		}
	}

protected:
	// The user supplied system
	UpdateFn update_func;

	// A tuple of the fully typed component pools used by this system
	TupPools const pools;

	// The pool that holds 'parent_id's
	component_pool<parent_id> const* pool_parent_id;

	// List of components used
	using component_list = type_list<FirstComponent, Components...>;
	using stripped_component_list = type_list<std::remove_cvref_t<FirstComponent>, std::remove_cvref_t<Components>...>;

	using user_interval = test_option_type_or<is_interval, Options, opts::interval<0, 0>>;
	using interval_type =
		std::conditional_t<(user_interval::_ecs_duration > 0.0),
						   interval_limiter<user_interval::_ecs_duration_ms, user_interval::_ecs_duration_us>, no_interval_limiter>;
	interval_type interval_checker;

	// Number of arguments
	static constexpr size_t num_arguments = 1 + sizeof...(Components);

	// Number of components
	static constexpr size_t num_components = sizeof...(Components) + !is_entity<FirstComponent>;

	// Number of filters
	static constexpr size_t num_filters = (std::is_pointer_v<FirstComponent> + ... + std::is_pointer_v<Components>);
	static_assert(num_filters < num_components, "systems must have at least one non-filter component");

	// Hashes of stripped types used by this system ('int' instead of 'int const&')
	static constexpr std::array<detail::type_hash, num_components> type_hashes =
		get_type_hashes_array<is_entity<FirstComponent>, std::remove_cvref_t<FirstComponent>, std::remove_cvref_t<Components>...>();

	//
	// ecs::parent related stuff

	// The index of potential ecs::parent<> component
	static constexpr int parent_index = test_option_index<is_parent, stripped_component_list>;
	static constexpr bool has_parent_types = (parent_index != -1);

	// The parent type, or void
	using full_parent_type = type_list_at_or<parent_index, component_list, void>;
	using stripped_parent_type = std::remove_cvref_t<full_parent_type>;
	using parent_component_list = parent_type_list_t<stripped_parent_type>;
	static constexpr int num_parent_components = type_list_size<parent_component_list>;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM
#ifndef ECS_SYSTEM_SORTED_H_
#define ECS_SYSTEM_SORTED_H_


namespace ecs::detail {
// Manages sorted arguments. Neither cache- nor storage space friendly, but arguments
// will be passed to the user supplied lambda in a sorted manner
template <typename Options, typename UpdateFn, typename SortFunc, class TupPools, class FirstComponent, class... Components>
struct system_sorted final : public system<Options, UpdateFn, TupPools, FirstComponent, Components...> {
	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

public:
	system_sorted(UpdateFn func, SortFunc sort, TupPools in_pools)
		: system<Options, UpdateFn, TupPools, FirstComponent, Components...>(func, in_pools), sort_func{sort} {}

private:
	void do_run() override {
		// Sort the arguments if the component data has been modified
		if (needs_sorting || std::get<pool<sort_types>>(this->pools)->has_components_been_modified()) {
			auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
			std::sort(e_p, arguments.begin(), arguments.end(), [this](auto const& l, auto const& r) {
				sort_types* t_l = std::get<sort_types*>(l);
				sort_types* t_r = std::get<sort_types*>(r);
				return sort_func(*t_l, *t_r);
			});

			needs_sorting = false;
		}

		auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
		std::for_each(e_p, arguments.begin(), arguments.end(), [this](auto packed_arg) {
			if constexpr (is_entity<FirstComponent>) {
				this->update_func(std::get<0>(packed_arg), extract_arg<Components>(packed_arg, 0)...);
			} else {
				this->update_func(extract_arg<FirstComponent>(packed_arg, 0), extract_arg<Components>(packed_arg, 0)...);
			}
		});
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build(entity_range_view entities) override {
		if (entities.size() == 0) {
			arguments.clear();
			return;
		}

		// Count the total number of arguments
		size_t arg_count = 0;
		for (auto const& range : entities) {
			arg_count += range.ucount();
		}

		// Reserve space for the arguments
		arguments.clear();
		arguments.reserve(arg_count);

		// Build the arguments for the ranges
		for (auto const& range : entities) {
			for (entity_id const& entity : range) {
				if constexpr (is_entity<FirstComponent>) {
					arguments.emplace_back(entity, get_component<Components>(entity, this->pools)...);
				} else {
					arguments.emplace_back(entity, get_component<FirstComponent>(entity, this->pools),
										   get_component<Components>(entity, this->pools)...);
				}
			}
		}

		needs_sorting = true;
	}

private:
	// The user supplied sorting function
	SortFunc sort_func;

	// The vector of unrolled arguments, sorted using 'sort_func'
	using argument = single_argument<FirstComponent, Components...>;
	std::vector<argument> arguments;

	// True if the data needs to be sorted
	bool needs_sorting = false;

	using sort_types = typename decltype(get_sorter_types(SortFunc{}))::type1;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_SORTED_H_
#ifndef ECS_SYSTEM_RANGED_H_
#define ECS_SYSTEM_RANGED_H_


namespace ecs::detail {
// Manages arguments using ranges. Very fast linear traversal and minimal storage overhead.
template <class Options, class UpdateFn, class TupPools, class FirstComponent, class... Components>
class system_ranged final : public system<Options, UpdateFn, TupPools, FirstComponent, Components...> {
	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

public:
	system_ranged(UpdateFn func, TupPools in_pools)
		: system<Options, UpdateFn, TupPools, FirstComponent, Components...>{func, in_pools}, walker{in_pools} {}

private:
	void do_run() override {
		auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc

		// Call the system for all the components that match the system signature
		for (auto const& argument : arguments) {
			entity_range const& range = std::get<entity_range>(argument);
			std::for_each(e_p, range.begin(), range.end(), [this, &argument, first_id = range.first()](auto ent) {
				auto const offset = ent - first_id;

				if constexpr (is_entity<FirstComponent>) {
					this->update_func(ent, extract_arg<Components>(argument, offset)...);
				} else {
					this->update_func(extract_arg<FirstComponent>(argument, offset), extract_arg<Components>(argument, offset)...);
				}
			});
		}
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build(entity_range_view entities) override {
		// Clear current arguments
		arguments.clear();

		// Reset the walker
		walker.reset(entities);

		while (!walker.done()) {
			if constexpr (is_entity<FirstComponent>) {
				arguments.emplace_back(walker.get_range(), walker.template get<Components>()...);
			} else {
				arguments.emplace_back(walker.get_range(), walker.template get<FirstComponent>(), walker.template get<Components>()...);
			}

			walker.next();
		}
	}

private:
	// Holds the arguments for a range of entities
	using argument_type = range_argument<FirstComponent, Components...>;
	std::vector<argument_type> arguments;

	pool_range_walker<TupPools> walker;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_RANGED_H_
#ifndef ECS_SYSTEM_HIERARCHY_H_
#define ECS_SYSTEM_HIERARCHY_H_




namespace ecs::detail {
template <class Options, class UpdateFn, class TupPools, class FirstComponent, class... Components>
class system_hierarchy final : public system<Options, UpdateFn, TupPools, FirstComponent, Components...> {
	using base = system<Options, UpdateFn, TupPools, FirstComponent, Components...>;

	// Determine the execution policy from the options (or lack thereof)
	using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(), std::execution::sequenced_policy,
												std::execution::parallel_policy>;

	struct entity_info {
		int parent_count = 0;
		entity_type root_id;
	};

	using info_map = std::unordered_map<entity_type, entity_info>;
	using info_iterator = typename info_map::const_iterator;

	using argument = decltype(
		std::tuple_cat(std::tuple<entity_id>{0}, std::declval<argument_tuple<FirstComponent, Components...>>(), std::tuple<entity_info>{}));

public:
	system_hierarchy(UpdateFn func, TupPools in_pools)
		: system<Options, UpdateFn, TupPools, FirstComponent, Components...>{func, in_pools}, parent_pools{make_parent_types_tuple()} {}

private:
	void do_run() override {
		auto const e_p = execution_policy{}; // cannot pass directly to 'for_each' in gcc
		std::for_each(e_p, argument_spans.begin(), argument_spans.end(), [this](auto const local_span) {
			for (argument& arg : local_span) {
				if constexpr (is_entity<FirstComponent>) {
					this->update_func(std::get<entity_id>(arg), extract<Components>(arg)...);
				} else {
					this->update_func(extract<FirstComponent>(arg), extract<Components>(arg)...);
				}
			}
		});
	}

	// Convert a set of entities into arguments that can be passed to the system
	void do_build(entity_range_view ranges) override {
		// Clear the arguments
		arguments.clear();
		argument_spans.clear();

		if (ranges.size() == 0) {
			return;
		}

		// Count the number of arguments to be constructed
		size_t count = 0;
		for (auto const& range : ranges)
			count += range.ucount();
		if constexpr (is_entity<FirstComponent>) {
			argument arg{entity_id{0}, component_argument<Components>{0}..., entity_info{}};
			arguments.resize(count, arg);
		} else {
			argument arg{entity_id{0}, component_argument<FirstComponent>{0}, component_argument<Components>{0}..., entity_info{}};
			arguments.resize(count, arg);
		}

		// TODO insert in set with top. ordering?

		// map of entity and root info
		std::map<entity_type, int> roots;

		// Build the arguments for the ranges
		std::atomic<int> index = 0;
		auto conv = entity_offset_conv{ranges};
		pool_entity_walker<TupPools> walker;
		info_map info;

		for (entity_range const& range : ranges) {
			walker.reset(&this->pools, entity_range_view{{range}});
			while (!walker.done()) {
				entity_id const entity = walker.get_entity();

				info_iterator const ent_info = fill_entity_info(info, entity, index);

				// Add the argument for the entity
				auto const ent_offset = static_cast<size_t>(conv.to_offset(entity));
				if constexpr (is_entity<FirstComponent>) {
					arguments[ent_offset] = argument(entity, walker.template get<Components>()..., ent_info->second);
				} else {
					arguments[ent_offset] =
						argument(entity, walker.template get<FirstComponent>(), walker.template get<Components>()..., ent_info->second);
				}

				// Update the root child count
				auto const root_index = ent_info->second.root_id;
				roots[root_index] += 1;

				walker.next();
			}
		}

		// Do the topological sort of the arguments
		std::sort(std::execution::par, arguments.begin(), arguments.end(), topological_sort_func);

		// Create the argument spans
		size_t offset = 0;
		for (auto const& [id, child_count] : roots) {
			argument_spans.emplace_back(arguments.data() + offset, child_count);
			offset += child_count;
		}
	}

	decltype(auto) make_parent_types_tuple() const {
		return apply_type<parent_component_list>([this]<typename... T>() {
			return std::make_tuple(&get_pool<std::remove_pointer_t<T>>(this->pools)...);
		});
	}

	// Extracts a component argument from a tuple
	template <typename Component, typename Tuple>
	static decltype(auto) extract(Tuple& tuple) {
		using T = std::remove_cvref_t<Component>;

		if constexpr (std::is_pointer_v<T>) {
			return nullptr;
		} else if constexpr (detail::is_parent<T>::value) {
			return std::get<T>(tuple);
		} else {
			T* ptr = std::get<T*>(tuple);
			return *(ptr);
		}
	}

	static bool topological_sort_func(argument const& arg_l, argument const& arg_r) {
		auto const& [depth_l, root_l] = std::get<entity_info>(arg_l);
		auto const& [depth_r, root_r] = std::get<entity_info>(arg_r);

		// order by roots
		if (root_l != root_r)
			return root_l < root_r;
		else
			// order by depth
			return depth_l < depth_r;
	}

	info_iterator fill_entity_info(info_map& info, entity_id const entity, std::atomic<int>& index) const {
		// Get the parent id
		entity_id const* parent_id = pool_parent_id->find_component_data(entity);
		if (parent_id == nullptr) {
			// This entity does not have a 'parent_id' component,
			// which means that this entity is a root
			auto const [it, _] = info.emplace(std::make_pair(entity, entity_info{0, index++}));
			return it;
		}

		// look up the parent info
		info_iterator parent_it = info.find(*parent_id);
		if (parent_it == info.end())
			parent_it = fill_entity_info(info, *parent_id, index);

		// insert the entity info
		auto const& [count, root_index] = parent_it->second;
		auto const [it, _p] = info.emplace(std::make_pair(entity, entity_info{1 + count, root_index}));
		return it;
	}

private:
	using base::has_parent_types;
	using base::num_parent_components;
	using base::parent_index;
	using base::pool_parent_id;
	using typename base::component_list;
	using typename base::full_parent_type;
	using typename base::parent_component_list;
	using typename base::stripped_component_list;
	using typename base::stripped_parent_type;

	// Ensure we have a parent type
	static_assert(has_parent_types, "no parent component found");

	// The vector of unrolled arguments
	std::vector<argument> arguments;

	// The spans over each tree in the argument vector
	std::vector<std::span<argument>> argument_spans;

	// A tuple of the fully typed component pools used the parent component
	parent_pool_tuple_t<stripped_parent_type> const parent_pools;
};
} // namespace ecs::detail

#endif // !ECS_SYSTEM_HIERARCHY_H_
#ifndef ECS_SYSTEM_GLOBAL_H
#define ECS_SYSTEM_GLOBAL_H


namespace ecs::detail {
// The implementation of a system specialized on its components
template <class Options, class UpdateFn, class TupPools, class FirstComponent, class... Components>
class system_global final : public system<Options, UpdateFn, TupPools, FirstComponent, Components...> {
public:
	system_global(UpdateFn func, TupPools in_pools)
		: system<Options, UpdateFn, TupPools, FirstComponent, Components...>{func, in_pools},
		  argument{&get_pool<FirstComponent>(in_pools).get_shared_component(), &get_pool<Components>(in_pools).get_shared_component()...} {}

private:
	void do_run() override {
		this->update_func(*std::get<std::remove_cvref_t<FirstComponent>*>(argument),
						  *std::get<std::remove_cvref_t<Components>*>(argument)...);
	}

	void do_build(entity_range_view) override {
		// Does nothing
	}

private:
	// The arguments for the system
	using global_argument = std::tuple<std::remove_cvref_t<FirstComponent>*, std::remove_cvref_t<Components>*...>;
	global_argument argument;
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
	std::map<type_hash, component_pool_base*> type_pool_lookup;
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

		auto constexpr process_changes = [](auto const& inst) { inst->process_changes(); };

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

		constexpr auto hash = get_type_hash<T>();
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

		type_caches.for_each([](auto& cache) { cache.reset(); });
		// type_caches.clear();  // DON'T! It will remove access to existing thread_local vars,
		// which means they can't be reached and reset
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

		constexpr auto hash = get_type_hash<T>();
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
	auto& create_system(UpdateFn update_func, SortFn sort_func, R(FirstArg, Args...)) {
		return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(update_func, sort_func);
	}

	// Const lambda with sort
	template <typename Options, typename UpdateFn, typename SortFn, typename R, typename C, typename FirstArg, typename... Args>
	auto& create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstArg, Args...) const) {
		return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(update_func, sort_func);
	}

	// Mutable lambda with sort
	template <typename Options, typename UpdateFn, typename SortFn, typename R, typename C, typename FirstComponent, typename... Components>
	auto& create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstComponent, Components...)) {
		return create_system<Options, UpdateFn, SortFn, FirstComponent, Components...>(update_func, sort_func);
	}

private:
	template <typename T, typename... R>
	auto make_tuple_pools() {
		using Tr = reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<T>>>;
		if constexpr (!is_entity<Tr>) {
			std::tuple<pool<Tr>, pool<reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<R>>>>...> t(
				&get_component_pool<Tr>(), &get_component_pool<reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<R>>>>()...);
			return t;
		} else {
			std::tuple<pool<reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<R>>>>...> t(
				&get_component_pool<reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<R>>>>()...);
			return t;
		}
	}

	template <typename BF, typename... B, typename... A>
	static auto tuple_cat_unique(std::tuple<A...> const& a, BF* const bf, B... b) {
		if constexpr ((std::is_same_v<BF* const, A> || ...)) {
			// BF exists in tuple a, so skip it
			(void)bf;
			if constexpr (sizeof...(B) > 0) {
				return tuple_cat_unique(a, b...);
			} else {
				return a;
			}
		} else {
			if constexpr (sizeof...(B) > 0) {
				return tuple_cat_unique(std::tuple_cat(a, std::tuple<BF* const>{bf}), b...);
			} else {
				return std::tuple_cat(a, std::tuple<BF* const>{bf});
			}
		}
	}

	template <typename Options, typename UpdateFn, typename SortFn, typename FirstComponent, typename... Components>
	auto& create_system(UpdateFn update_func, SortFn sort_func) {
		// Find potential parent type
		using parent_type =
			test_option_type_or<is_parent, type_list<std::remove_cvref_t<FirstComponent>, std::remove_cvref_t<Components>...>, void>;

		// Do some checks on the systems
		bool constexpr has_sort_func = !std::is_same_v<SortFn, std::nullptr_t>;
		bool constexpr has_parent = !std::is_same_v<void, parent_type>;
		bool constexpr is_global_sys = detail::global<FirstComponent> && (detail::global<Components> && ...);

		// Global systems cannot have a sort function
		static_assert(!(is_global_sys == has_sort_func && is_global_sys), "Global systems can not be sorted");

		static_assert(!(has_sort_func == has_parent && has_parent == true), "Systems can not both be hierarchial and sorted");

		// Create the system instance
		std::unique_ptr<system_base> sys;
		if constexpr (has_parent) {

			// Find the component pools
			auto const all_pools = apply_type<parent_type_list_t<parent_type>>([&]<typename... T>() {
				// The pools for the regular components
				auto const pools = make_tuple_pools<FirstComponent, Components...>();

				// Add the pools for the parents components
				if constexpr (sizeof...(T) > 0) {
					return tuple_cat_unique(pools,
											&get_component_pool<reduce_parent_t<std::remove_pointer_t<std::remove_cvref_t<T>>>>()...);
				} else {
					return pools;
				}
			});

			using typed_system = system_hierarchy<Options, UpdateFn, decltype(all_pools), FirstComponent, Components...>;
			sys = std::make_unique<typed_system>(update_func, all_pools);
		} else if constexpr (is_global_sys) {
			auto const pools = make_tuple_pools<FirstComponent, Components...>();
			using typed_system = system_global<Options, UpdateFn, decltype(pools), FirstComponent, Components...>;
			sys = std::make_unique<typed_system>(update_func, pools);
		} else if constexpr (has_sort_func) {
			auto const pools = make_tuple_pools<FirstComponent, Components...>();
			using typed_system = system_sorted<Options, UpdateFn, SortFn, decltype(pools), FirstComponent, Components...>;
			sys = std::make_unique<typed_system>(update_func, sort_func, pools);
		} else {
			auto const pools = make_tuple_pools<FirstComponent, Components...>();
			using typed_system = system_ranged<Options, UpdateFn, decltype(pools), FirstComponent, Components...>;
			sys = std::make_unique<typed_system>(update_func, pools);
		}

		std::unique_lock system_lock(system_mutex);
		sys->process_changes(true);
		systems.push_back(std::move(sys));
		detail::system_base* ptr_system = systems.back().get();
		Ensures(ptr_system != nullptr);

		bool constexpr request_manual_update = has_option<opts::manual_update, Options>();
		if constexpr (!request_manual_update)
			sched.insert(ptr_system);

		return *ptr_system;
	}

	// Create a component pool for a new type
	template <typename T>
	component_pool_base* create_component_pool() {
		// Create a new pool
		auto pool = std::make_unique<component_pool<T>>();
		constexpr auto hash = get_type_hash<T>();
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
		static_assert(detail::unique<First, T...>, "the same component was specified more than once");
		static_assert(!detail::global<First> && (!detail::global<T> && ...), "can not add global components to entities");
		static_assert(!std::is_pointer_v<std::remove_cvref_t<First>> && (!std::is_pointer_v<std::remove_cvref_t<T>> && ...),
					  "can not add pointers to entities; wrap them in a struct");

		auto const adder = [this, range]<class Type>(Type&& val) {
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
		static_assert(!detail::is_parent<std::remove_cvref_t<T>>::value, "adding spans of parents is not (yet?) supported");
		static_assert(std::copyable<T>, "Type must be copyable");

		// Add it to the component pool
		detail::component_pool<T>& pool = ctx.get_component_pool<T>();
		pool.add_span(range, std::span{vals});
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
	size_t get_component_count() {
		if (!ctx.has_component_pool<T>())
			return 0;

		// Get the component pool
		detail::component_pool<T> const& pool = ctx.get_component_pool<T>();
		return pool.num_components();
	}

	// Returns the number of entities that has the component.
	template <typename T>
	size_t get_entity_count() {
		if (!ctx.has_component_pool<T>())
			return 0;

		// Get the component pool
		detail::component_pool<T> const& pool = ctx.get_component_pool<T>();
		return pool.num_entities();
	}

	// Return true if an entity contains the component
	template <typename T>
	bool has_component(entity_id const id) {
		if (!ctx.has_component_pool<T>())
			return false;

		detail::component_pool<T> const& pool = ctx.get_component_pool<T>();
		return pool.has_entity(id);
	}

	// Returns true if all entities in a range has the component.
	template <typename T>
	bool has_component(entity_range const range) {
		if (!ctx.has_component_pool<T>())
			return false;

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
	auto& make_system(SystemFunc sys_func, SortFn sort_func = nullptr) {
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
	/*template <class Component>
	void set_memory_resource(std::pmr::memory_resource* resource) {
		auto& pool = ctx.get_component_pool<Component>();
		pool.set_memory_resource(resource);
	}

	// Returns the memory resource used to store a specific type of component
	template <class Component>
	std::pmr::memory_resource* get_memory_resource() {
		auto& pool = ctx.get_component_pool<Component>();
		return pool.get_memory_resource();
	}

	// Resets the memory resource to the default
	template <class Component>
	void reset_memory_resource() {
		auto& pool = ctx.get_component_pool<Component>();
		pool.set_memory_resource(std::pmr::get_default_resource());
	}*/

private:
	detail::context ctx;
};
} // namespace ecs

#endif // !ECS_RUNTIME

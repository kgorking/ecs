#include <algorithm> 
#include <concepts> 
#include <execution> 
#include <functional> 
#include <map> 
#include <optional> 
#include <shared_mutex> 
#include <span> 
#include <type_traits> 
#include <tuple> 
#include <typeinfo> 
#include <typeindex> 
#include <utility> 
#include <variant> 
#include <vector> 
 
// Contracts. If they are violated, the program is an invalid state, so nuke it from orbit 
#define Expects(cond) ((cond) ? static_cast<void>(0) : std::terminate()) 
#define Ensures(cond) ((cond) ? static_cast<void>(0) : std::terminate()) 
 
 
 
#ifndef __ENTITY_ID
#define __ENTITY_ID

namespace ecs {
	using entity_type = int;
	using entity_offset = std::ptrdiff_t; // can cover the entire entity_type domain

	// A simple struct that is an entity identifier.
	// Use a struct so the typesystem can differentiate
	// between entity ids and regular integers in system arguments
	struct entity_id final {
		// Uninitialized entity ids are not allowed, because they make no sense
		entity_id() = delete;

		constexpr entity_id(entity_type _id) noexcept
			: id(_id) {
		}

		constexpr operator entity_type& () noexcept { return id; }
		constexpr operator entity_type () const noexcept { return id; }

	private:
		entity_type id;
	};
}

#endif // !__ENTITY_ID
 
#ifndef __ENTITY
#define __ENTITY

namespace ecs {
	// A simple helper class for easing the adding and removing of components
	class entity final {
		entity_id ent;

	public:
		constexpr entity(entity_id ent)
			: ent(ent) {
		}

		template <std::copyable ...Components>
		entity(entity_id ent, Components&&... components)
			: ent(ent) {
			add<Components...>(std::forward<Components>(components)...);
		}

		template <std::copyable ...Components>
		void add(Components&&... components) const {
			add_components(ent, std::forward<Components>(components)...);
		}

		template <std::copyable ...Components>
		void add() const {
			add_components(ent, Components{}...);
		}

		template <typename ...Components>
		void remove() const {
			(remove_component<Components>(ent), ...);
		}

		template <typename ...Component>
		[[nodiscard]] bool has() const {
			return (has_component<Component>(ent) && ...);
		}

		template <typename Component>
		[[nodiscard]] Component& get() const {
			return get_component<Component>(ent);
		}

		[[nodiscard]] constexpr entity_id get_id() const {
			return ent;
		}
	};
}

#endif // !__ENTITY
 
#ifndef __ENTITY_RANGE
#define __ENTITY_RANGE

namespace ecs {
	// Defines a range of entities.
	// 'last' is included in the range.
	class entity_range final {
		entity_id first_;
		entity_id last_;

	public:
		// Iterator support
		// TODO harden
		class iterator {
			entity_id ent_ = std::numeric_limits<entity_type>::min();  // has to be default initialized due to msvc parallel implementation of for_each, which is annoying

		public:
			// iterator traits
			using difference_type = entity_type;
			using value_type = entity_id;
			using pointer = const entity_id*;
			using reference = const entity_id&;
			using iterator_category = std::random_access_iterator_tag;

			//iterator() = delete; // no such thing as a 'default' entity
			iterator() noexcept = default;
			constexpr iterator(entity_id ent) noexcept : ent_(ent) {}
			constexpr iterator& operator++() { ent_++; return *this; }
			constexpr iterator operator++(int) { iterator const retval = *this; ++(*this); return retval; }
			constexpr iterator operator+(difference_type diff) const { return { ent_ + diff }; }
			constexpr difference_type operator-(difference_type diff) const { return ent_ - diff; }
			//constexpr iterator operator+(iterator in_it) const { return { ent_.id + in_it.ent_.id }; }
			constexpr difference_type operator-(iterator in_it) const { return ent_ - in_it.ent_; }
			constexpr bool operator==(iterator other) const { return ent_ == other.ent_; }
			constexpr bool operator!=(iterator other) const { return !(*this == other); }
			constexpr entity_id operator*() { return ent_; }
		};
		[[nodiscard]] constexpr iterator begin() const { return { first_ }; }
		[[nodiscard]] constexpr iterator end() const { return { last_ + 1 }; }

	public:
		entity_range() = delete; // what is a default range?

		constexpr entity_range(entity_id first, entity_id last)
			: first_(first)
			, last_(last) {
			Expects(first <= last);
		}

		// Construct an entity range and add components to them.
		template <typename ...Components>
		entity_range(entity_id first, entity_id last, Components&& ... components)
			: first_(first)
			, last_(last) {
			Expects(first <= last);
			add<Components...>(std::forward<Components>(components)...);
		}

		template <typename ...Components>
		void add(Components&& ... components) const {
			add_components(*this, std::forward<Components>(components)...);
		}

		template <typename ...Components>
		void add() const {
			add_components(*this, Components{}...);
		}

		template <std::copyable ...Components>
		void remove() const {
			(remove_component<Components>(*this), ...);
		}

		template <std::copyable ...Components>
		[[nodiscard]] bool has() const {
			return (has_component<Components>(*this) && ...);
		}

		template <std::copyable Component>
		[[nodiscard]] std::span<Component> get() const {
			return std::span(get_component<Component>(first_), count());
		}

		constexpr bool operator == (entity_range const& other) const {
			return equals(other);
		}

		// For sort
		constexpr bool operator <(entity_range const& other) const {
			return first_ < other.first() && last_ < other.last();
		}

		// Returns the first entity in the range
		[[nodiscard]] constexpr entity_id first() const {
			return first_;
		}

		// Returns the last entity in the range
		[[nodiscard]] constexpr entity_id last() const {
			return last_;
		}

		// Returns the number of entities in this range
		[[nodiscard]] constexpr size_t count() const {
			Expects(last_ >= first_);
			return static_cast<size_t>(last_) - first_ + 1;
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

		// Returns the offset of an entity into this range
		// Pre: 'ent' must be in the range
		[[nodiscard]] constexpr entity_offset offset(entity_id const ent) const {
			Expects(contains(ent));
			return static_cast<entity_offset>(ent) - first_;
		}

		[[nodiscard]] constexpr bool can_merge(entity_range const& other) const {
			return last_ + 1 == other.first();
		}

		[[nodiscard]] constexpr bool overlaps(entity_range const& other) const {
			return first_ <= other.last_ && other.first_ <= last_;
		}

		// Removes a range from another range.
		// If the range was split by the remove, it returns two ranges.
		// Pre: 'other' must be contained in 'range', but must not be equal to it
		[[nodiscard]] constexpr static std::pair<entity_range, std::optional<entity_range>> remove(entity_range const& range, entity_range const& other) {
			Expects(range.contains(other));
			Expects(!range.equals(other));

			// Remove from the front
			if (other.first() == range.first()) {
				return {
					entity_range{ other.last() + 1, range.last() },
					std::nullopt
				};
			}

			// Remove from the back
			if (other.last() == range.last()) {
				return {
					entity_range{ range.first(), other.first() - 1 },
					std::nullopt
				};
			}

			// Remove from the middle
			return {
				entity_range{ range.first(), other.first() - 1 },
				entity_range{ other.last() + 1, range.last() }
			};
		}

		// Combines two ranges into one
		// Pre: r1 and r2 must be adjacent ranges, r1 < r2
		[[nodiscard]] constexpr static entity_range merge(entity_range const& r1, entity_range const& r2) {
			Expects(r1.can_merge(r2));
			return entity_range{ r1.first(), r2.last() };
		}

		// Returns the intersection of two ranges
		// Pre: The ranges must overlap, the resulting ranges can not have zero-length
		[[nodiscard]] constexpr static entity_range intersect(entity_range const& range, entity_range const& other) {
			Expects(range.overlaps(other));

			entity_id const first{ std::max(range.first(), other.first()) };
			entity_id const last{ std::min(range.last(),  other.last()) };
			Expects(last - first >= 0);

			return entity_range{ first, last };
		}
	};

	using entity_range_view = std::span<entity_range const>;
}

#endif // !__ENTITTY_RANGE
 
#ifndef __THREADED
#define __THREADED

#include <mutex>
#include <vector>
#include <list>
#include <algorithm>

// Provides a thread-local instance of the type T for each thread that
// accesses it. The set of instances can be accessed through the begin()/end() iterators.
template <typename T>
class threaded {
	// This struct manages the instances that access the thread-local data.
	// Its lifetime is marked as thread_local, which means that it can live longer than
	// the threaded<> instance that spawned it.
	struct instance_access {
		// Return reference to this instances local data
		T& get(threaded<T>* instance) {
			if (instance != owner) {
				// First-time access
				owner = instance;
				data = instance->init_thread(this);
			}

			return *data;
		}

		void remove(threaded<T>* instance) noexcept {
			if (owner == instance) {
				owner = nullptr;
				data = nullptr;
			}
		}

	private:
		threaded<T>* owner{};
		T* data{};
	};
	friend instance_access;

	// Adds a instance_access and allocates its data.
	// Returns a pointer to the data
	T* init_thread(instance_access* t) {
		// Mutex for serializing access for creating/removing locals
		static std::mutex mtx_storage;
		std::scoped_lock sl(mtx_storage);

		instances.push_back(t);
		data.emplace_front() = T{};
		return &data.front();
	}

	// Remove the threal_local_data. The data itself is preserved
	void remove_thread(instance_access* t) noexcept {
		// Remove the instance_access from the vector
		auto it = std::find(instances.begin(), instances.end(), t);
		if (it != instances.end()) {
			std::swap(*it, instances.back());
			instances.pop_back();
		}
	}

public:
	~threaded() noexcept {
		clear();
	}

	auto begin() noexcept { return data.begin(); }
	auto end() noexcept { return data.end(); }
	auto cbegin() const noexcept { return data.cbegin(); }
	auto cend() const noexcept { return data.cend(); }

	// Get the thread-local instance of T for the current instance
	T& local() {
		thread_local instance_access var{};
		T& t = var.get(this);
		return t;
	}

	// Clears all the thread instances
	void clear() noexcept {
		for (instance_access* instance : instances)
			instance->remove(this);
		instances.clear();
		data.clear();
	}

private:
	// the threads that access this instance
	std::vector<instance_access*> instances;

	// instance-data created by each thread. list contents are not invalidated when more items are added, unlike a vector
	std::list<T> data;
};
#endif // !__THREADED 
#ifndef __COMPONENT_SPECIFIER
#define __COMPONENT_SPECIFIER

namespace ecs {
	// Add this in 'ecs_flags()' to mark a component as a tag.
	// Uses O(1) memory instead of O(n).
	// Mutually exclusive with 'share'
	struct tag {};

	// Add this in 'ecs_flags()' to mark a component as shared between components,
	// meaning that any entity with a shared component will all point to the same component.
	// Think of it as a static member variable in a regular class.
	// Uses O(1) memory instead of O(n).
	// Mutually exclusive with 'tag'
	struct share {};

	// Add this in 'ecs_flags()' to mark a component as transient.
	// The component will only exist on an entity for one cycle,
	// and then be automatically removed.
	struct transient {};

	// Add this in 'ecs_flags()' to mark a component as constant.
	// A compile-time error will be raised if a system tries to
	// access the component through a non-const reference.
	struct immutable {};

	// Add flags to a component to change its behaviour and memory usage.
	// Example:
	// struct my_component { 
	// 	ecs_flags(ecs::tag, ecs::transient);
	// 	// component data
	// };
#define ecs_flags(...) struct _ecs_flags : __VA_ARGS__ {};

// Some helpers
	namespace detail {
		template <typename T>
		using flags = typename std::remove_cvref_t<T>::_ecs_flags;

		template<typename T> concept tagged = std::is_base_of_v<ecs::tag, flags<T>>;
		template<typename T> concept shared = std::is_base_of_v<ecs::share, flags<T>>;
		template<typename T> concept transient = std::is_base_of_v<ecs::transient, flags<T>>;
		template<typename T> concept immutable = std::is_base_of_v<ecs::immutable, flags<T>>;

		template<typename T> concept persistent = !transient<T>;
		template<typename T> concept unbound = (shared<T> || tagged<T>); // component is not bound to a specific entity (ie static)
	}
}

#endif // !__COMPONENT_SPECIFIER
 
#ifndef __COMPONENT_POOL_BASE
#define __COMPONENT_POOL_BASE

namespace ecs::detail {
	// The baseclass of typed component pools
	// TODO try and get rid of this baseclass
	class component_pool_base {
	public:
		component_pool_base() = default;
		component_pool_base(component_pool_base const&) = delete;
		component_pool_base(component_pool_base&&) = delete;
		component_pool_base& operator = (component_pool_base const&) = delete;
		component_pool_base& operator = (component_pool_base&&) = delete;
		virtual ~component_pool_base() = default;

		virtual void process_changes() = 0;
		virtual void clear_flags() = 0;
		virtual void clear() = 0;
	};
}

#endif // !__COMPONENT_POOL_BASE
 
#ifndef __COMPONENT_POOL
#define __COMPONENT_POOL

namespace ecs::detail {
	// For std::visit
	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

	template <std::copyable T>
	class component_pool final : public component_pool_base {
	private:
		// The components
		std::vector<T> components;

		// The entities that have components in this storage.
		std::vector<entity_range> ranges;

		// Keep track of which components to add/remove each cycle
		using variant = std::variant<T, std::function<T(entity_id)>>;
		using entity_data = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, variant>>;
		threaded<std::vector<entity_data>> deferred_adds;
		threaded<std::vector<entity_range>> deferred_removes;

		// Status flags
		bool data_added = false;
		bool data_removed = false;

	public:
		// Add a component to a range of entities, initialized by the supplied user function
		// Pre: entities has not already been added, or is in queue to be added
		//      This condition will not be checked until 'process_changes' is called.
		template <typename Fn> requires (!unbound<T>)
		void add_init(entity_range const range, Fn&& init) {
			// Add the range and function to a temp storage
			deferred_adds.local().emplace_back(range, std::forward<Fn>(init));
		}

		// Add a shared/tagged component to a range of entity.
		// Pre: entities has not already been added, or is in queue to be added
		//      This condition will not be checked until 'process_changes' is called.
		void add(entity_range const range, T&& /* unused */) requires unbound<T> {
			deferred_adds.local().push_back(range);
		}

		// Add a component to a range of entity.
		// Pre: entities has not already been added, or is in queue to be added
		//      This condition will not be checked until 'process_changes' is called.
		void add(entity_range const range, T&& component) {
			deferred_adds.local().emplace_back(range, std::forward<T>(component));
		}

		// Add a component to an entity.
		// Pre: entity has not already been added, or is in queue to be added.
		//      This condition will not be checked until 'process_changes' is called.
		void add(entity_id const id, T&& component) {
			add({ id, id }, std::forward<T>(component));
		}

		// Return the shared component
		T& get_shared_component() requires unbound<T> {
			static T t;
			return t;
		}

		// Remove an entity from the component pool. This logically removes the component from the entity.
		void remove(entity_id const id) {
			remove_range({ id, id });
		}

		// Remove an entity from the component pool. This logically removes the component from the entity.
		void remove_range(entity_range const range) {
			if (!has_entity(range)) {
				return;
			}

			auto& rem = deferred_removes.local();
			if (!rem.empty() && rem.back().can_merge(range)) {
				rem.back() = entity_range::merge(rem.back(), range);
			}
			else {
				rem.push_back(range);
			}
		}

		// Returns the shared component
		T* find_component_data(entity_id const /*id*/) requires unbound<T> {
			return &get_shared_component();
		}

		// Returns an entities component.
		// Returns nullptr if the entity is not found in this pool
		T* find_component_data(entity_id const id) {
			auto const index = find_entity_index(id);
			return index ? &components[index.value()] : nullptr;
		}

		// Merge all the components queued for addition to the main storage,
		// and remove components queued for removal
		void process_changes() override {
			process_remove_components();
			process_add_components();
		}

		// Returns the number of active entities in the pool
		size_t num_entities() const {
			return std::accumulate(ranges.begin(), ranges.end(), size_t{ 0 }, [](size_t val, entity_range const& range) { return val + range.count(); });
		}

		// Returns the number of active components in the pool
		size_t num_components() const {
			if constexpr (unbound<T>)
				return 1;
			else
				return components.size();
		}

		// Clears the pools state flags
		void clear_flags() override {
			data_added = false;
			data_removed = false;
		}

		// Returns true if components has been added since last clear_flags() call
		bool is_data_added() const {
			return data_added;
		}

		// Returns true if components has been removed since last clear_flags() call
		bool is_data_removed() const {
			return data_removed;
		}

		// Returns true if components has been added/removed since last clear_flags() call
		bool is_data_modified() const {
			return data_added || data_removed;
		}

		// Returns the pools entities
		entity_range_view get_entities() const {
			return ranges;
		}

		// Returns true if an entity has a component in this pool
		bool has_entity(entity_id const id) const {
			return has_entity({ id, id });
		}

		// Returns true if an entity range has components in this pool
		bool has_entity(entity_range const& range) const {
			if (ranges.empty()) {
				return false;
			}

			for (entity_range const& r : ranges) {
				if (r.contains(range)) {
					return true;
				}
			}

			return false;
		}

		// Checks the current threads queue for the entity
		bool is_queued_add(entity_id const id) {
			return is_queued_add({ id, id });
		}

		// Checks the current threads queue for the entity
		bool is_queued_add(entity_range const& range) {
			if (deferred_adds.local().empty()) {
				return false;
			}

			for (auto const& ents : deferred_adds.local()) {
				if (std::get<0>(ents).contains(range)) {
					return true;
				}
			}

			return false;
		}

		// Checks the current threads queue for the entity
		bool is_queued_remove(entity_id const id) {
			return is_queued_remove({ id, id });
		}

		// Checks the current threads queue for the entity
		bool is_queued_remove(entity_range const& range) {
			if (deferred_removes.local().empty())
				return false;

			for (auto const& ents : deferred_removes.local()) {
				if (ents.contains(range))
					return true;
			}

			return false;
		}

		// Clear all entities from the pool
		void clear() override {
			// Remember if components was removed from the pool
			bool const is_removed = !components.empty();

			// Clear the pool
			ranges.clear();
			components.clear();
			deferred_adds.clear();
			deferred_removes.clear();
			clear_flags();

			// Save the removal state
			data_removed = is_removed;
		}

	private:
		// Flag that components has been added
		void set_data_added() {
			data_added = true;
		}

		// Flag that components has been removed
		void set_data_removed() {
			data_removed = true;
		}

		// Searches for an entitys offset in to the component pool.
		// Returns nothing if 'ent' is not a valid entity
		std::optional<size_t> find_entity_index(entity_id const ent) const {
			if (ranges.empty() || !has_entity(ent)) {
				return {};
			}

			// Run through the ranges
			size_t index = 0;
			for (entity_range const& range : ranges) {
				if (!range.contains(ent)) {
					index += range.count();
					continue;
				}

				index += range.offset(ent);
				return index;
			}

			return {};
		}

		// Add new queued entities and components to the main storage
		void process_add_components() {
			// Combine the components in to a single vector
			std::vector<entity_data> adds;
			for (auto& vec : deferred_adds) {
				std::move(vec.begin(), vec.end(), std::back_inserter(adds));
			}

			if (adds.empty()) {
				return;
			}

			// Clear the current adds
			deferred_adds.clear();

			// Sort the input
			auto constexpr comparator = [](entity_data const& l, entity_data const& r) {
				return std::get<0>(l).first() < std::get<0>(r).first();
			};
			if (!std::is_sorted(adds.begin(), adds.end(), comparator)) {
				std::sort(adds.begin(), adds.end(), comparator);
			}

			// Check the 'add*' functions precondition.
			// An entity can not have more than one of the same component
			auto const has_duplicate_entities = [](auto const& vec) {
				return vec.end() != std::adjacent_find(vec.begin(), vec.end(), [](auto const& l, auto const& r) {
					return std::get<0>(l) == std::get<0>(r);
				});
			};
			Expects(false == has_duplicate_entities(adds));

			// Small helper function for combining ranges
			auto const add_range = [](std::vector<entity_range>& dest, entity_range const& range) {
				// Merge the range or add it
				if (!dest.empty() && dest.back().can_merge(range)) {
					dest.back() = entity_range::merge(dest.back(), range);
				}
				else {
					dest.push_back(range);
				}
			};

			// Add the new entities/components
			std::vector<entity_range> new_ranges;
			auto ranges_it = ranges.cbegin();
			[[maybe_unused]] auto component_it = components.cbegin();
			for (auto& add : adds) {
				entity_range const& range = std::get<0>(add);

				// Copy the current ranges while looking for an insertion point
				while (ranges_it != ranges.cend() && (*ranges_it < range)) {
					if constexpr (!unbound<T>) {
						// Advance the component iterator so it will point to the correct components when i start inserting it
						component_it += ranges_it->count();
					}

					add_range(new_ranges, *ranges_it++);
				}

				// New range must not already exist in the pool
				if (ranges_it != ranges.cend())
					Expects(false == ranges_it->overlaps(range));

				// Add the new range
				add_range(new_ranges, range);

				if constexpr (!unbound<T>) {
					auto const add_val = [this, &component_it, range](T&& val) {
						component_it = components.insert(component_it, range.count(), std::forward<T>(val));
						component_it = std::next(component_it, range.count());
					};
					auto const add_init = [this, &component_it, range](std::function<T(entity_id)> init) {
						for (entity_id ent = range.first(); ent <= range.last(); ++ent) {
							component_it = components.emplace(component_it, init(ent));
							component_it = std::next(component_it);
						}
					};

					// Add the new components
					std::visit(overloaded{ add_val, add_init }, std::move(std::get<1>(add)));
				}
			}

			// Move the remaining ranges
			std::move(ranges_it, ranges.cend(), std::back_inserter(new_ranges));

			// Store the new ranges
			ranges = std::move(new_ranges);

			// Update the state
			set_data_added();
		}

		// Removes the entities and components
		void process_remove_components() {
			// Transient components are removed each cycle
			if constexpr (detail::transient<T>) {
				if (!ranges.empty()) {
					ranges.clear();
					components.clear();
					set_data_removed();
				}
			}
			else {
				// Combine the vectors
				std::vector<entity_range> removes;
				for (auto& vec : deferred_removes) {
					std::move(vec.begin(), vec.end(), std::back_inserter(removes));
				}

				if (removes.empty()) {
					return;
				}

				// Clear the current removes
				deferred_removes.clear();

				// Sort it if needed
				if (!std::is_sorted(removes.begin(), removes.end())) {
					std::sort(removes.begin(), removes.end());
				}

				// An entity can not have more than one of the same component
				auto const has_duplicate_entities = [](auto const& vec) {
					return vec.end() != std::adjacent_find(vec.begin(), vec.end());
				};
				Expects(false == has_duplicate_entities(removes));

				// Remove the components
				if constexpr (!unbound<T>) {
					// Find the first valid index
					auto index = find_entity_index(removes.front().first());
					Expects(index.has_value());
					auto dest_it = components.begin() + index.value();
					auto from_it = dest_it + removes.front().count();

					if (dest_it == components.begin() && from_it == components.end()) {
						components.clear();
					}
					else {
						// Move components between the ranges
						for (auto it = removes.cbegin() + 1; it != removes.cend(); ++it) {
							index = find_entity_index(it->first());
							Expects(index.has_value());

							auto const last_it = components.begin() + index.value();
							auto const dist = std::distance(from_it, last_it);
							from_it = std::move(from_it, last_it, dest_it);
							dest_it += dist;
						}

						// Move rest of components
						auto const dist = std::distance(from_it, components.end());
						std::move(from_it, components.end(), dest_it);
						dest_it += dist;

						// Erase the unused space
						components.erase(dest_it, components.end());
					}
				}

				// Remove the ranges
				auto curr_range = ranges.begin();
				for (auto const& remove : removes) {
					// Step forward until a candidate range is found
					while (!curr_range->contains(remove) && curr_range != ranges.end()) {
						++curr_range;
					}

					if (curr_range == ranges.end()) {
						break;
					}

					// Erase the current range if it equals the range to be removed
					if (curr_range->equals(remove)) {
						curr_range = ranges.erase(curr_range);
					}
					else {
						// Do the removal
						auto result = entity_range::remove(*curr_range, remove);

						// Update the modified range
						*curr_range = result.first;

						// If the range was split, add the other part of the range
						if (result.second.has_value()) {
							curr_range = ranges.insert(curr_range + 1, result.second.value());
						}
					}
				}

				// Update the state
				set_data_removed();
			}
		}
	};
}

#endif // !__COMPONENT_POOL
 
#ifndef __SYSTEM_VERIFICATION
#define __SYSTEM_VERIFICATION

namespace ecs::detail {
	// Given a type T, if it is callable with an entity argument,
	// resolve to the return type of the callable. Otherwise assume the type T.
	template<typename T>
	struct get_type {
		using type = T;
	};

	template<typename T> requires std::invocable<T, int>
	struct get_type<T> {
		using type = std::invoke_result_t<T, int>;
	};

	template<typename T>
	using get_type_t = typename get_type<T>::type;

	// Count the number of times the type F appears in the parameter pack T
	template<typename F, typename... T>
	constexpr int count_types = (std::is_same_v<get_type_t<F>, get_type_t<T>> +... + 0);

	// Ensure that any type in the parameter pack T is only present once.
	template<typename... T>
	concept unique = ((count_types<T, T...> == 1) && ...);

	template <class T>
	concept entity_type = std::is_same_v<std::remove_cvref_t<T>, entity_id> || std::is_same_v<std::remove_cvref_t<T>, entity>;

	template <class R, class FirstArg, class ...Args>
	concept checked_system = requires {
		// systems can not return values
		requires std::is_same_v<R, void>;

		// no pointers allowed
		requires !std::is_pointer_v<FirstArg> && (!std::is_pointer_v<Args>&& ...);

		// systems must take at least one component argument
		requires (entity_type<FirstArg> ? sizeof...(Args) >= 1 : true);

		// Make sure the first entity is not passed as a reference
		requires (entity_type<FirstArg> ? !std::is_reference_v<FirstArg> : true);

		// Component types can only be specified once
		requires unique<FirstArg, Args...>;

		// Components flagged as 'immutable' must also be const
		requires
			(detail::immutable<FirstArg> ? std::is_const_v<std::remove_reference_t<FirstArg>> : true) &&
			((detail::immutable<Args> ? std::is_const_v<std::remove_reference_t<Args>> : true) && ...);

		// Components flagged as 'tag' must not be references
		requires
			(detail::tagged<FirstArg> ? !std::is_reference_v<FirstArg> : true) &&
			((detail::tagged<Args> ? !std::is_reference_v<Args> : true) && ...);

		// Components flagged as 'tag' must not hold data
		requires
			(detail::tagged<FirstArg> ? sizeof(FirstArg) == 1 : true) &&
			((detail::tagged<Args> ? sizeof(Args) == 1 : true) && ...);

		// Components flagged as 'share' must not be 'tag'ged
		requires
			(detail::shared<FirstArg> ? !detail::tagged<FirstArg> : true) &&
			((detail::shared<Args> ? !detail::tagged<Args> : true) && ...);
	};

	// A small bridge to allow the Lambda concept to activate the system concept
	template <class R, class C, class FirstArg, class ...Args>
		requires (checked_system<R, FirstArg, Args...>)
	struct lambda_to_system_bridge {
		lambda_to_system_bridge(R(C::*)(FirstArg, Args...)) {};
		lambda_to_system_bridge(R(C::*)(FirstArg, Args...) const) {};
		lambda_to_system_bridge(R(C::*)(FirstArg, Args...) noexcept) {};
		lambda_to_system_bridge(R(C::*)(FirstArg, Args...) const noexcept) {};
	};

	template <typename T>
	concept lambda = requires {
		// Must have the call operator
		&T::operator ();

		// Check all the system requirements
		lambda_to_system_bridge(&T::operator ());
	};
}

#endif // !__SYSTEM_VERIFICATION
 
#ifndef __SYSTEM
#define __SYSTEM

namespace ecs::detail {
	class context;
}

namespace ecs {
	class system {
	public:
		system() = default;
		virtual ~system() = default;
		system(system const&) = delete;
		system(system&&) = default;
		system& operator =(system const&) = delete;
		system& operator =(system&&) = default;

		// Run this system on all of its associated components
		virtual void update() = 0;

		// Enables this system for updates and runs
		void enable() { set_enable(true); }

		// Prevent this system from being updated or run
		void disable() { set_enable(false); }

		// Sets wheter the system is enabled or disabled
		void set_enable(bool is_enabled) {
			enabled = is_enabled;
			if (is_enabled) {
				process_changes(true);
			}
		}

		// Returns true if this system is enabled
		[[nodiscard]] bool is_enabled() const { return enabled; }

		// Returns the group this system belongs to
		[[nodiscard]] virtual int get_group() const noexcept = 0;

	private:
		// Only allow the context class to call 'process_changes'
		friend class detail::context;

		// Process changes to component layouts
		virtual void process_changes(bool force_rebuild = false) = 0;


		// Whether this system is enabled or disabled. Disabled systems are neither updated nor run.
		bool enabled = true;
	};
}

#endif // !__SYSTEM
 
#ifndef __SYSTEM_IMPL
#define __SYSTEM_IMPL

namespace ecs::detail {
	// The implementation of a system specialized on its components
	template <int Group, class ExecutionPolicy, typename UserUpdateFunc, class FirstComponent, class ...Components>
	class system_impl final : public system {
		// Determines if the first component is an entity
		static constexpr bool is_first_arg_entity = std::is_same_v<FirstComponent, entity_id> || std::is_same_v<FirstComponent, entity>;

		// Calculate the number of components
		static constexpr size_t num_components = sizeof...(Components) + (is_first_arg_entity ? 0 : 1);

		// The first type in the system, entity or component
		using first_type = std::conditional_t<is_first_arg_entity, FirstComponent, FirstComponent*>;

		// Alias for stored pools
		template <class T>
		using pool = component_pool<T>* const;

		// Tuple holding all pools used by this system
		using tup_pools = std::conditional_t<is_first_arg_entity,
			std::tuple<                      pool<Components>...>,
			std::tuple<pool<FirstComponent>, pool<Components>...>>;

		// Holds an entity range and a pointer to the first component from each pool in that range
		using range_arguments = std::conditional_t<is_first_arg_entity,
			std::tuple<entity_range, Components* ...>,
			std::tuple<entity_range, FirstComponent*, Components* ...>>;

		// Holds the arguments for a range of entities
		std::vector<range_arguments> arguments;

		// A tuple of the fully typed component pools used by this system
		tup_pools const pools;

		// The user supplied system
		UserUpdateFunc update_func;

		// Execution policy
		ExecutionPolicy exe_pol{};

	public:
		// Constructor for when the first argument to the system is _not_ an entity
		system_impl(UserUpdateFunc update_func, pool<FirstComponent> first_pool, pool<Components>... pools)
			: pools{ first_pool, pools... }
			, update_func{ update_func }
		{
			build_args();
		}

		// Constructor for when the first argument to the system _is_ an entity
		system_impl(UserUpdateFunc update_func, pool<Components> ... pools)
			: pools{ pools... }
			, update_func{ update_func }
		{
			build_args();
		}

		void update() override {
			if (!is_enabled()) {
				return;
			}

			// Call the system for all pairs of components that match the system signature
			for (auto const& argument : arguments) {
				auto const& range = std::get<entity_range>(argument);
				std::for_each(exe_pol, range.begin(), range.end(), [this, &argument, first_id = range.first()](auto ent) {
					// Small helper function
					auto const extract_arg = [](auto ptr, [[maybe_unused]] ptrdiff_t offset) {
						using T = std::remove_cvref_t<decltype(*ptr)>;
						if constexpr (detail::unbound<T>) {
							return ptr;
						}
						else {
							return ptr + offset;
						}
					};

					auto const offset = ent - first_id;
					if constexpr (is_first_arg_entity) {
						update_func(ent,
									*extract_arg(std::get<Components*>(argument), offset)...);
					}
					else {
						update_func(*extract_arg(std::get<FirstComponent*>(argument), offset),
									*extract_arg(std::get<Components*>(argument), offset)...);
					}
				});
			}
		}

		[[nodiscard]] constexpr int get_group() const noexcept override {
			return Group;
		}

	private:
		// Handle changes when the component pools change
		void process_changes(bool force_rebuild) override {
			if (force_rebuild) {
				build_args();
				return;
			}

			if (!is_enabled()) {
				return;
			}

			auto constexpr is_pools_modified = [](auto ...pools) { return (pools->is_data_modified() || ...); };
			bool const is_modified = std::apply(is_pools_modified, pools);

			if (is_modified) {
				build_args();
			}
		}

		void build_args() {
			entity_range_view const entities = std::get<0>(pools)->get_entities();

			if constexpr (num_components == 1) {
				// Build the arguments
				build_args(entities);
			}
			else {
				// When there are more than one component required for a system,
				// find the intersection of the sets of entities that have those components

				auto constexpr do_intersection = [](entity_range_view initial, entity_range_view first, auto ...rest) {
					// Intersects two ranges of entities
					auto constexpr intersector = [](entity_range_view view_a, entity_range_view view_b) {
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

							if (it_a->last() < it_b->last()) { // range a is inside range b, move to the next range in a
								++it_a;
							}
							else if (it_b->last() < it_a->last()) { // range b is inside range a, move to the next range in b
								++it_b;
							}
							else { // ranges are equal, move to next ones
								++it_a;
								++it_b;
							}
						}

						return result;
					};

					std::vector<entity_range> intersect = intersector(initial, first);
					((intersect = intersector(intersect, rest)), ...);
					return intersect;
				};

				// Build the arguments
				auto const intersect = do_intersection(entities, get_pool<Components>().get_entities()...);
				build_args(intersect);
			}
		}

		// Convert a set of entities into arguments that can be passed to the system
		void build_args(entity_range_view entities) {
			// Build the arguments for the ranges
			arguments.clear();
			for (auto const& range : entities) {
				if constexpr (is_first_arg_entity) {
					arguments.emplace_back(range, get_component<Components>(range.first())...);
				}
				else {
					arguments.emplace_back(range, get_component<FirstComponent>(range.first()), get_component<Components>(range.first())...);
				}
			}
		}

		template <typename Component>
		[[nodiscard]] component_pool<Component>& get_pool() const {
			return *std::get<pool<Component>>(pools);
		}

		template <typename Component>
		[[nodiscard]] Component* get_component(entity_id const entity) {
			return get_pool<Component>().find_component_data(entity);
		}
	};
}

#endif // !__SYSTEM_IMPL
 
#ifndef __CONTEXT
#define __CONTEXT

namespace ecs::detail {
	// The central class of the ecs implementation. Maintains the state of the system.
	class context final {
		// The values that make up the ecs core.
		std::vector<std::unique_ptr<system>> systems;
		std::vector<std::unique_ptr<component_pool_base>> component_pools;
		std::map<std::type_index, component_pool_base*> type_pool_lookup;

		mutable std::shared_mutex mutex;

	public:
		// Commits the changes to the entities.
		void commit_changes() {
			// Prevent other threads from
			//  adding components
			//  registering new component types
			//  adding new systems
			std::shared_lock lock(mutex);

			// Let the component pools handle pending add/remove requests for components
			for (auto const& pool : component_pools) {
				pool->process_changes();
			}

			// Let the systems respond to any changes in the component pools
			for (auto const& sys : systems) {
				sys->process_changes();
			}

			// Reset any dirty flags on pools
			for (auto const& pool : component_pools) {
				pool->clear_flags();
			}
		}

		// Calls the 'update' function on all the systems in the order they were added.
		void run_systems() {
			// Prevent other threads from adding new systems
			std::shared_lock lock(mutex);

			for (auto const& sys : systems) {
				sys->update();
			}
		}

		// Returns true if a pool for the type exists
		bool has_component_pool(std::type_info const& type) const {
			// Prevent other threads from registering new component types
			std::shared_lock lock(mutex);

			return type_pool_lookup.contains(std::type_index(type));
		}

		// Resets the runtime state. Removes all systems, empties component pools
		void reset() {
			std::unique_lock lock(mutex);

			systems.clear();
			// context::component_pools.clear(); // this will cause an exception in get_component_pool() due to the cache
			for (auto& pool : component_pools) {
				pool->clear();
			}
		}

		// Returns a reference to a components pool.
		// If a pool doesn't exist, one will be created.
		template <typename T>
		component_pool<T>& get_component_pool() {
			// Simple thread-safe caching, ~15% performance boost in benchmarks
			struct internal_dummy__ {};
			thread_local std::type_index last_type{ typeid(internal_dummy__) };		// init to a function-local type
			thread_local component_pool_base* last_pool{};

			auto const type_index = std::type_index(typeid(T));
			if (type_index != last_type) {
				// Look in the pool for the type
				std::shared_lock lock(mutex);
				auto const it = type_pool_lookup.find(type_index);

				[[unlikely]] if (it == type_pool_lookup.end()) {
					// The pool wasn't found so create it.
					// create_component_pool takes a unique lock, so unlock the
					// shared lock during its call
					lock.unlock();
					last_pool = create_component_pool<T>();
					Ensures(last_pool != nullptr);
				}
				else {
					last_pool = it->second;
					Ensures(last_pool != nullptr);
				}
				last_type = type_index;
			}

			return *static_cast<component_pool<T>*>(last_pool);
		}

		// Const lambdas
		template <int Group, typename ExecutionPolicy, typename UserUpdateFunc, typename R, typename C, typename ...Args>
		auto& create_system(UserUpdateFunc update_func, R(C::*)(Args...) const) {
			return create_system_impl<Group, ExecutionPolicy, UserUpdateFunc, Args...>(update_func);
		}

		// Mutable lambdas
		template <int Group, typename ExecutionPolicy, typename UserUpdateFunc, typename R, typename C, typename ...Args>
		auto& create_system(UserUpdateFunc update_func, R(C::*)(Args...)) {
			return create_system_impl<Group, ExecutionPolicy, UserUpdateFunc, Args...>(update_func);
		}

	private:
		template <int Group, typename ExecutionPolicy, typename UserUpdateFunc, typename FirstArg, typename ...Args>
		auto& create_system_impl(UserUpdateFunc update_func) {
			// Set up the implementation
			using typed_system_impl = system_impl<Group, ExecutionPolicy, UserUpdateFunc, std::remove_cv_t<std::remove_reference_t<FirstArg>>, std::remove_cv_t<std::remove_reference_t<Args>>...>;

			// Is the first argument an entity of sorts?
			bool constexpr has_entity = std::is_same_v<FirstArg, entity_id> || std::is_same_v<FirstArg, entity>;

			// Create the system instance
			std::unique_ptr<system> sys;
			if constexpr (has_entity) {
				sys = std::make_unique<typed_system_impl>(
					update_func,
					/* dont add the entity as a component pool */
					&get_component_pool<std::remove_cv_t<std::remove_reference_t<Args>>>()...);
			}
			else {
				sys = std::make_unique<typed_system_impl>(
					update_func,
					&get_component_pool<std::remove_cv_t<std::remove_reference_t<FirstArg>>>(),
					&get_component_pool<std::remove_cv_t<std::remove_reference_t<Args>>>()...);
			}

			std::unique_lock lock(mutex);
			systems.push_back(std::move(sys));
			system* ptr_system = systems.back().get();
			Ensures(ptr_system != nullptr);

			sort_systems_by_group();

			return *ptr_system;
		}

		// Sorts the systems based on their group number.
		// The sort maintains ordering in the individual groups.
		void sort_systems_by_group() {
			std::stable_sort(systems.begin(), systems.end(), [](auto const& l, auto const& r) {
				return l->get_group() < r->get_group();
			});
		}

		// Create a component pool for a new type
		template <typename T>
		component_pool_base* create_component_pool() {
			// Create a new pool if one does not already exist
			auto const& type = typeid(T);
			if (!has_component_pool(type)) {
				std::unique_lock lock(mutex);

				auto pool = std::make_unique<component_pool<T>>();
				type_pool_lookup.emplace(std::type_index(type), pool.get());
				component_pools.push_back(std::move(pool));
				return component_pools.back().get();
			}
			else
				return &get_component_pool<T>();
		}
	};

	inline context& get_context() {
		static context ctx;
		return ctx;
	}

	// The global reference to the context
	static inline context& _context = get_context();
}

#endif // !__CONTEXT
 
#ifndef __RUNTIME
#define __RUNTIME

namespace ecs {
	// Add components generated from an initializer function to a range of entities. Will not be added until 'commit_changes()' is called.
	// The initializer function signature must be
	//   T(ecs::entity_id)
	// where T is the component type returned by the function.
	// Pre: entity does not already have the component, or have it in queue to be added
	template <typename Callable> requires std::invocable<Callable, entity_id>
	void add_component(entity_range const range, Callable&& func) {
		// Return type of 'func'
		using ComponentType = decltype(std::declval<Callable>()(entity_id{ 0 }));
		static_assert(!std::is_same_v<ComponentType, void>, "Initializer functions must return a component");

		// Add it to the component pool
		detail::component_pool<ComponentType>& pool = detail::_context.get_component_pool<ComponentType>();
		pool.add_init(range, std::forward<Callable>(func));
	}

	// Add a component to a range of entities. Will not be added until 'commit_changes()' is called.
	// Pre: entity does not already have the component, or have it in queue to be added
	template <typename T>
	void add_component(entity_range const range, T&& val) {
		static_assert(!std::is_reference_v<T>, "can not store references; pass a copy instead");
		static_assert(std::copyable<T>, "T must be copyable");

		// Add it to the component pool
		detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
		pool.add(range, std::forward<T>(val));
	}

	// Add a component to an entity. Will not be added until 'commit_changes()' is called.
	// Pre: entity does not already have the component, or have it in queue to be added
	template <typename T>
	void add_component(entity_id const id, T&& val) {
		add_component({ id, id }, std::forward<T>(val));
	}

	// Add several components to a range of entities. Will not be added until 'commit_changes()' is called.
	// Pre: entity does not already have the component, or have it in queue to be added
	template <typename ...T>
	void add_components(entity_range const range, T&&... vals) {
		static_assert(detail::unique<T...>, "the same component was specified more than once");
		(add_component(range, std::forward<T>(vals)), ...);
	}

	// Add several components to an entity. Will not be added until 'commit_changes()' is called.
	// Pre: entity does not already have the component, or have it in queue to be added
	template <typename ...T>
	void add_components(entity_id const id, T&&... vals) {
		static_assert(detail::unique<T...>, "the same component was specified more than once");
		(add_component(id, std::forward<T>(vals)), ...);
	}

	// Removes a component from a range of entities. Will not be removed until 'commit_changes()' is called.
	// Pre: entity has the component
	template <detail::persistent T>
	void remove_component(entity_range const range) {
		// Remove the entities from the components pool
		detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
		pool.remove_range(range);
	}

	// Removes a component from an entity. Will not be removed until 'commit_changes()' is called.
	// Pre: entity has the component
	template <typename T>
	void remove_component(entity_id const id) {
		remove_component<T>({ id, id });
	}

	// Removes all components from an entity
	/*inline void remove_all_components(entity_id const id)
	{
		for (auto const& pool : context::internal::component_pools)
			pool->remove(id);
	}*/

	// Returns a shared component. Can be called before a system for it has been added
	template <detail::shared T>
	T& get_shared_component() {
		return detail::_context.get_component_pool<T>().get_shared_component();
	}

	// Returns the component from an entity, or nullptr if the entity is not found
	template <typename T>
	T* get_component(entity_id const id) {
		// Get the component pool
		detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
		return pool.find_component_data(id);
	}

	// Returns the components from an entity range, or an empty span if the entities are not found
	// or does not containg the component.
	// The span might be invalidated after a call to 'ecs::commit_changes()'.
	template <typename T>
	std::span<T> get_components(entity_range const range) {
		if (!has_component<T>(range))
			return {};

		// Get the component pool
		detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
		return { pool.find_component_data(range.first()), static_cast<ptrdiff_t>(range.count()) };
	}

	// Returns the number of active components
	template <typename T>
	size_t get_component_count() {
		if (!detail::_context.has_component_pool(typeid(T)))
			return 0;

		// Get the component pool
		detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
		return pool.num_components();
	}

	// Returns the number of entities that has the component.
	template <typename T>
	size_t get_entity_count() {
		if (!detail::_context.has_component_pool(typeid(T)))
			return 0;

		// Get the component pool
		detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
		return pool.num_entities();
	}

	// Return true if an entity contains the component
	template <typename T>
	bool has_component(entity_id const id) {
		if (!detail::_context.has_component_pool(typeid(T)))
			return false;

		detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
		return pool.has_entity(id);
	}

	// Returns true if all entities in a range has the component.
	template <typename T>
	bool has_component(entity_range const range) {
		if (!detail::_context.has_component_pool(typeid(T)))
			return false;

		detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
		return pool.has_entity(range);
	}

	// Commits the changes to the entities.
	inline void commit_changes() {
		detail::_context.commit_changes();
	}

	// Calls the 'update' function on all the systems in the order they were added.
	inline void run_systems() {
		detail::_context.run_systems();
	}

	// Commits all changes and calls the 'update' function on all the systems in the order they were added.
	// Same as calling commit_changes() and run_systems().
	inline void update_systems() {
		commit_changes();
		run_systems();
	}

	// Make a new system
	template <int Group = 0, detail::lambda UserUpdateFunc>
	auto& make_system(UserUpdateFunc update_func) {
		return detail::_context.create_system<Group, std::execution::sequenced_policy, UserUpdateFunc>(update_func, &UserUpdateFunc::operator());
	}

	// Make a new system. It will process components in parallel.
	template <int Group = 0, detail::lambda UserUpdateFunc>
	auto& make_parallel_system(UserUpdateFunc update_func) {
		return detail::_context.create_system<Group, std::execution::parallel_unsequenced_policy, UserUpdateFunc>(update_func, &UserUpdateFunc::operator());
	}
}

#endif // !__RUNTIME

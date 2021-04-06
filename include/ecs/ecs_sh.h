#ifndef ECS_TLS_CACHE
#define ECS_TLS_CACHE

#include <algorithm>

namespace tls {
    // A class using a cache-line to cache data
    template <class Key, class Value, Key empty_slot = Key{}, size_t cache_line = 64UL>
    class cache {
    public:
        static constexpr size_t max_entries = (cache_line) / (sizeof(Key) + sizeof(Value));

        cache() noexcept {
            reset();
        }

        template <class Fn>
        Value get_or(Key const& k, Fn or_fn) {
            auto const index = find_index(k);
            if (index < max_entries)
                return values[index];

            insert_val(k, or_fn(k));
            return values[0];
        }

        void reset() {
            std::fill(keys, keys + max_entries, empty_slot);
            std::fill(values, values + max_entries, Value{});
        }

    protected:
        void insert_val(Key const& k, Value v) {
            // Move all but last pair one step to the right
            //std::shift_right(keys, keys + max_entries, 1);
            //std::shift_right(values, values + max_entries, 1);
            std::move_backward(keys, keys + max_entries - 1, keys + max_entries);
            std::move_backward(values, values + max_entries - 1, values + max_entries);

            // Insert the new pair at the front of the cache
            keys[0] = k;
            values[0] = std::move(v);
        }

        std::size_t find_index(Key const& k) const {
            auto const it = std::find(keys, keys + max_entries, k);
            if (it == keys + max_entries)
                return max_entries;
            return std::distance(keys, it);
        }

    private:
        Key keys[max_entries];
        Value values[max_entries];
    };

}

#endif // !ECS_TLS_CACHE
#ifndef ECS_TLS_SPLITTER_H
#define ECS_TLS_SPLITTER_H

#include <mutex>
#include <vector>
#include <forward_list>
#include <algorithm>
#ifdef _MSC_VER
#include <new>  // for std::hardware_destructive_interference_size
#endif

namespace tls {

    namespace detail {
        // Cache-line aware allocator for std::forward_list
        // This is needed to ensure that data is forced into seperate cache lines
        template<class T> // T is the list node containing the type
        struct cla_forward_list_allocator {
            using value_type = T;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;
            using is_always_equal = std::true_type;

#ifdef _MSC_VER
            static constexpr auto cache_line_size = std::hardware_destructive_interference_size;
            static constexpr auto alloc_size = sizeof(T); // msvc already keeps nodes on seperate cache lines
#else // gcc/clang (libc++) needs some masaging
            static constexpr auto cache_line_size = 64UL; // std::hardware_destructive_interference_size not implemented in libc++
            static constexpr auto fl_node_size = 16UL; // size of a forward_list node
            static constexpr auto alloc_size = std::max(sizeof(T), cache_line_size - fl_node_size);
#endif

            constexpr cla_forward_list_allocator() noexcept {}
            constexpr cla_forward_list_allocator(const cla_forward_list_allocator&) noexcept = default;
            template <class other>
            constexpr cla_forward_list_allocator(const cla_forward_list_allocator<other>&) noexcept {}

            T* allocate(std::size_t n) {
                return static_cast<T*>(malloc(n * alloc_size));
            }

            void deallocate(T* p, std::size_t /*n*/) {
                free(p);
            }
        };
    }

    // Provides a thread-local instance of the type T for each thread that
    // accesses it. This avoid having to use locks to read/write data.
    // This class only locks when a new thread is created/destroyed.
    // The set of instances can be accessed through the begin()/end() iterators.
    // Note: Two splitter<T> instances in the same thread will point to the same data.
    //       Differentiate between them by passing different types to 'UnusedDifferentiaterType'.
    //       As the name implies, it's not used internally, so just put whatever.
    template <typename T, typename UnusedDifferentiaterType = void>
    class splitter {
        // This struct manages the instances that access the thread-local data.
        // Its lifetime is marked as thread_local, which means that it can live longer than
        // the splitter<> instance that spawned it.
        struct instance_access {
            // Return a reference to an instances local data
            T& get(splitter<T, UnusedDifferentiaterType>* instance) {
                if (owner == nullptr) {
                    // First-time access
                    owner = instance;
                    data = instance->init_thread(this);
                }

                return *data;
            }

            void remove(splitter<T, UnusedDifferentiaterType>* instance) noexcept {
                if (owner == instance) {
                    owner = nullptr;
                    data = nullptr;
                }
            }

        private:
            splitter<T, UnusedDifferentiaterType>* owner{};
            T* data{};
        };
        friend instance_access;

        // the threads that access this instance
        std::vector<instance_access*> instances;

        // instance-data created by each thread.
        // list contents are not invalidated when more items are added, unlike a vector
        std::forward_list<T, detail::cla_forward_list_allocator<T>> data;

protected:
        // Adds a instance_access and allocates its data.
        // Returns a pointer to the data
        T* init_thread(instance_access* t) {
            // Mutex for serializing access for creating/removing locals
            static std::mutex mtx_storage;
            std::scoped_lock sl(mtx_storage);

            instances.push_back(t);
            data.emplace_front(T{});
            return &data.front();
        }

        // Remove the instance_access. The data itself is preserved
        void remove_thread(instance_access* t) noexcept {
            auto it = std::find(instances.begin(), instances.end(), t);
            if (it != instances.end()) {
                std::swap(*it, instances.back());
                instances.pop_back();
            }
        }

    public:
        ~splitter() noexcept {
            clear();
        }

        using iterator = typename decltype(data)::iterator;
        using const_iterator = typename decltype(data)::const_iterator;

        iterator begin() noexcept { return data.begin(); }
        iterator end() noexcept { return data.end(); }
        const_iterator begin() const noexcept { return data.begin(); }
        const_iterator end() const noexcept { return data.end(); }

        // Get the thread-local instance of T for the current instance
        T& local() {
            thread_local instance_access var{};
            return var.get(this);
        }

        // Clears all the thread instances and data
        void clear() noexcept {
            for (instance_access* instance : instances)
                instance->remove(this);
            instances.clear();
            data.clear();
        }

        // Sort the data using std::less<>
        void sort() {
            data.sort();
        }

        // Sort the data using supplied predicate 'bool(auto const&, auto const&)'
        template <class Predicate>
        void sort(Predicate pred) {
            data.sort(pred);
        }
    };
}

#endif // !ECS_TLS_SPLITTER_H
#ifndef ECS_CONTRACT
#define ECS_CONTRACT

// Contracts. If they are violated, the program is an invalid state, so nuke it from orbit
#define Expects(cond) do { ((cond) ? static_cast<void>(0) : std::terminate()); } while(false)
#define Ensures(cond) do { ((cond) ? static_cast<void>(0) : std::terminate()); } while(false)

#endif // !ECS_CONTRACT
#ifndef ECS_TYPE_HASH
#define ECS_TYPE_HASH

#include <cstdint>
#include <string_view>

// Beware of using this with local defined structs/classes
// https://developercommunity.visualstudio.com/content/problem/1010773/-funcsig-missing-data-from-locally-defined-structs.html

namespace ecs::detail {
    using type_hash = std::uint64_t;

    template<class T>
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

    template<class T>
    constexpr type_hash get_type_hash() {
        constexpr type_hash prime = 0x100000001b3;
#ifdef _MSC_VER
        constexpr std::string_view string = __FUNCDNAME__; // has full type info, but is not very readable
#else
        constexpr std::string_view string = __PRETTY_FUNCTION__ ;
#endif

        type_hash hash = 0xcbf29ce484222325;
        for (auto const value : string) {
            hash ^= value;
            hash *= prime;
        }

        return hash;
    }

    template<bool ignore_first_arg, typename First, typename... Types>
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
    struct entity_id final {
        // Uninitialized entity ids are not allowed, because they make no sense
        entity_id() = delete;

        constexpr entity_id(detail::entity_type _id) noexcept
            : id(_id) {
        }

        constexpr operator detail::entity_type &() noexcept {
            return id;
        }
        constexpr operator detail::entity_type() const noexcept {
            return id;
        }

    private:
        detail::entity_type id;
    };
} // namespace ecs

#endif // !ECS_ENTITY_ID
#ifndef ECS_ENTITY_ITERATOR
#define ECS_ENTITY_ITERATOR

#include <iterator>
#include <limits>

namespace ecs::detail {

    // Iterator support
    class entity_iterator {
    public:
        // iterator traits
        using difference_type = entity_offset;
        using value_type = entity_type;
        using pointer = const entity_type*;
        using reference = const entity_type&;
        using iterator_category = std::random_access_iterator_tag;

        // entity_iterator() = delete; // no such thing as a 'default' entity
        constexpr entity_iterator() noexcept {};

        constexpr entity_iterator(entity_id ent) noexcept
            : ent_(ent) {
        }

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
        constexpr entity_type step(entity_type start, entity_offset diff) const {
            // ensures the value wraps instead of causing an overflow
            auto const diff_start = static_cast<entity_offset>(start);
            return static_cast<entity_type>(diff_start + diff);
        }

    private:
        value_type ent_{0};
    };
} // namespace ecs

#endif // !ECS_ENTITY_RANGE
#ifndef ECS_ENTITY_RANGE
#define ECS_ENTITY_RANGE

#include <algorithm>
#include <limits>
#include <optional>
#include <span>


namespace ecs {
    // Defines a range of entities.
    // 'last' is included in the range.
    class entity_range final {
        detail::entity_type first_;
        detail::entity_type last_;

    public:
        entity_range() = delete; // no such thing as a 'default' range

        constexpr entity_range(detail::entity_type first, detail::entity_type last)
            : first_(first)
            , last_(last) {
            Expects(first <= last);
        }

        template<typename Component>
        [[nodiscard]] std::span<Component> get() const {
            return std::span(get_component<Component>(first_), count());
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

        // Returns the first entity in the range
        [[nodiscard]] constexpr entity_id first() const {
            return entity_id{first_};
        }

        // Returns the last entity in the range
        [[nodiscard]] constexpr entity_id last() const {
            return entity_id{last_};
        }

        // Returns the number of entities in this range
        [[nodiscard]] constexpr size_t count() const {
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
        [[nodiscard]] constexpr detail::entity_offset offset(entity_id const ent) const {
            Expects(contains(ent));
            return static_cast<detail::entity_offset>(ent) - first_;
        }

        [[nodiscard]] constexpr bool can_merge(entity_range const& other) const {
            return last_ + 1 == other.first();
        }

        [[nodiscard]] constexpr bool overlaps(entity_range const& other) const {
            return first_ <= other.last_ && other.first_ <= last_;
        }

        // Removes a range from another range.
        // If the range was split by the remove, it returns two ranges.
        // Pre: 'other' must overlap 'range', but must not be equal to it
        [[nodiscard]] constexpr static std::pair<entity_range, std::optional<entity_range>> remove(
            entity_range const& range, entity_range const& other) {
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
                return {entity_range{range.first(), other.first() - 1},
                    entity_range{other.last() + 1, range.last()}};
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
        [[nodiscard]] constexpr static entity_range merge(
            entity_range const& r1, entity_range const& r2) {
            Expects(r1.can_merge(r2));
            return entity_range{r1.first(), r2.last()};
        }

        // Returns the intersection of two ranges
        // Pre: The ranges must overlap
        [[nodiscard]] constexpr static entity_range intersect(
            entity_range const& range, entity_range const& other) {
            Expects(range.overlaps(other));

            entity_id const first{std::max(range.first(), other.first())};
            entity_id const last{std::min(range.last(), other.last())};

            return entity_range{first, last};
        }
    };

    // The view of a collection of ranges
    using entity_range_view = std::span<entity_range const>;

} // namespace ecs

#endif // !ECS_ENTITTY_RANGE
#ifndef ECS_COMPONENT_SPECIFIER
#define ECS_COMPONENT_SPECIFIER

#include <type_traits>

namespace ecs {
    namespace flag {
        // Add this in a component with 'ecs_flags()' to mark it as tag.
        // Uses O(1) memory instead of O(n).
        // Mutually exclusive with 'share' and 'global'
        struct tag{};

        // Add this in a component with 'ecs_flags()' to mark it as shared.
        // Any entity with a shared component will all point to the same component.
        // Think of it as a static member variable in a regular class.
        // Uses O(1) memory instead of O(n).
        // Mutually exclusive with 'tag' and 'global'
        struct share{};

        // Add this in a component with 'ecs_flags()' to mark it as transient.
        // The component will only exist on an entity for one cycle,
        // and then be automatically removed.
        // Mutually exclusive with 'global'
        struct transient{};

        // Add this in a component with 'ecs_flags()' to mark it as constant.
        // A compile-time error will be raised if a system tries to
        // access the component through a non-const reference.
        struct immutable{};

        // Add this in a component with 'ecs_flags()' to mark it as global.
        // Global components can be referenced from systems without
        // having been added to any entities.
        // Uses O(1) memory instead of O(n).
        // Mutually exclusive with 'tag', 'share', and 'transient'
        struct global{};
    }

// Add flags to a component to change its behaviour and memory usage.
// Example:
// struct my_component {
// 	ecs_flags(ecs::flag::tag, ecs::flag::transient);
// 	// component data
// };
#define ecs_flags(...)                                                                                                 \
    struct _ecs_flags : __VA_ARGS__ {}

    namespace detail {
        // Some helpers

        template<typename T>
        using flags = typename std::remove_cvref_t<T>::_ecs_flags;

        template<typename T>
        concept tagged = std::is_base_of_v<ecs::flag::tag, flags<T>>;

        template<typename T>
        concept shared = std::is_base_of_v<ecs::flag::share, flags<T>>;

        template<typename T>
        concept transient = std::is_base_of_v<ecs::flag::transient, flags<T>>;

        template<typename T>
        concept immutable = std::is_base_of_v<ecs::flag::immutable, flags<T>>;

        template<typename T>
        concept global = std::is_base_of_v<ecs::flag::global, flags<T>>;

        template<typename T>
        concept local = !global<T> && !shared<T>;

        template<typename T>
        concept persistent = !transient<T>;

        template<typename T>
        concept unbound = (shared<T> || tagged<T> ||
                           global<T>); // component is not bound to a specific entity (ie static)
    }                                  // namespace detail
} // namespace ecs

#endif // !ECS_COMPONENT_SPECIFIER
#ifndef ECS_COMPONENT_POOL_BASE
#define ECS_COMPONENT_POOL_BASE

namespace ecs::detail {
    // The baseclass of typed component pools
    // TODO try and get rid of this baseclass
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
#ifndef ECS_COMPONENT_POOL
#define ECS_COMPONENT_POOL

#include <functional>
#include <tuple>
#include <type_traits>
#include <vector>
#include <execution>
#include <cstring> // for memcmp



template<class ForwardIt, class BinaryPredicate>
ForwardIt std_combine_erase(ForwardIt first, ForwardIt last, BinaryPredicate p) {
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

template<class Cont, class BinaryPredicate>
void combine_erase(Cont& cont, BinaryPredicate p) {
    auto const end = std_combine_erase(cont.begin(), cont.end(), p);
    cont.erase(end, cont.end());
}

namespace ecs::detail {
    template<typename T>
    class component_pool final : public component_pool_base {
    private:
        // The components
        std::vector<T> components;

        // The entities that have components in this storage.
        std::vector<entity_range> ranges;

        // Keep track of which components to add/remove each cycle
        using entity_data = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, T>>;
        using entity_init = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, std::function<T(entity_id)>>>;
        tls::splitter<std::vector<entity_data>, component_pool<T>> deferred_adds;
        tls::splitter<std::vector<entity_init>, component_pool<T>> deferred_init_adds;
        tls::splitter<std::vector<entity_range>, component_pool<T>> deferred_removes;

        // Status flags
        bool components_added = false;
        bool components_removed = false;
        bool components_modified = false;

    public:
        // Add a component to a range of entities, initialized by the supplied user function
        // Pre: entities has not already been added, or is in queue to be added
        //      This condition will not be checked until 'process_changes' is called.
        template<typename Fn>
        void add_init(entity_range const range, Fn&& init) {
            // Add the range and function to a temp storage
            deferred_init_adds.local().emplace_back(range, std::forward<Fn>(init));
        }

        // Add a component to a range of entity.
        // Pre: entities has not already been added, or is in queue to be added
        //      This condition will not be checked until 'process_changes' is called.
        void add(entity_range const range, T&& component) {
            if constexpr (shared<T> || tagged<T>) {
                deferred_adds.local().push_back(range);
            } else {
                deferred_adds.local().emplace_back(range, std::forward<T>(component));
            }
        }

        // Return the shared component
        T& get_shared_component() requires unbound<T> {
            static T t{};
            return t;
        }

        // Remove an entity from the component pool. This logically removes the component from the
        // entity.
        void remove(entity_id const id) {
            remove_range({id, id});
        }

        // Remove an entity from the component pool. This logically removes the component from the
        // entity.
        void remove_range(entity_range const range) {
            deferred_removes.local().push_back(range);
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
            // Don't want to include <algorithm> just for this
            size_t val = 0;
            for (auto r : ranges) {
                val += r.count();
            }
            return val;
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
            components_added = false;
            components_removed = false;
            components_modified = false;
        }

        // Returns true if components has been added since last clear_flags() call
        bool has_more_components() const {
            return components_added;
        }

        // Returns true if components has been removed since last clear_flags() call
        bool has_less_components() const {
            return components_removed;
        }

        // Returns true if components has been added/removed since last clear_flags() call
        bool has_component_count_changed() const {
            return components_added || components_removed;
        }

        bool has_components_been_modified() const {
            return has_component_count_changed() || components_modified;
        }

        // Returns the pools entities
        entity_range_view get_entities() const {
            if constexpr (detail::global<T>) {
                // globals are accessible to all entities
                static constexpr entity_range global_range{
                    std::numeric_limits<ecs::detail::entity_type>::min(), std::numeric_limits<ecs::detail::entity_type>::max()};
                return entity_range_view{&global_range, 1};
            } else {
                return ranges;
            }
        }

        // Returns true if an entity has a component in this pool
        bool has_entity(entity_id const id) const {
            return has_entity({id, id});
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

        // TODO remove?
        // Checks the current threads queue for the entity
        bool is_queued_add(entity_id const id) {
            return is_queued_add({id, id});
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
            return is_queued_remove({id, id});
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
            deferred_init_adds.clear();
            deferred_removes.clear();
            clear_flags();

            // Save the removal state
            components_removed = is_removed;
        }

        // Flag that components has been modified
        void notify_components_modified() {
            components_modified = true;
        }

    private:
        // Flag that components has been added
        void set_data_added() {
            components_added = true;
        }

        // Flag that components has been removed
        void set_data_removed() {
            components_removed = true;
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

            std::vector<entity_init> inits;
            for (auto& vec : deferred_init_adds) {
                std::move(vec.begin(), vec.end(), std::back_inserter(inits));
            }

            if (adds.empty() && inits.empty()) {
                return;
            }

            // Clear the current adds
            deferred_adds.clear();
            deferred_init_adds.clear();

            // Sort the input
            auto constexpr comparator = [](auto const& l, auto const& r) {
                return std::get<0>(l).first() < std::get<0>(r).first();
            };
            std::sort(std::execution::par, adds.begin(), adds.end(), comparator);
            std::sort(std::execution::par, inits.begin(), inits.end(), comparator);

            // Check the 'add*' functions precondition.
            // An entity can not have more than one of the same component
            auto const has_duplicate_entities = [](auto const& vec) {
                return vec.end() != std::adjacent_find(vec.begin(), vec.end(),
                                        [](auto const& l, auto const& r) { return std::get<0>(l) == std::get<0>(r); });
            };
            Expects(false == has_duplicate_entities(adds));

            // Merge adjacent ranges
            if constexpr (!detail::unbound<T>) { // contains data
                combine_erase(adds, [](entity_data& a, entity_data const& b) {
                    auto& [a_rng, a_data] = a;
                    auto const& [b_rng, b_data] = b;

                    if (a_rng.can_merge(b_rng) && 0 == memcmp(&a_data, &b_data, sizeof(T))) {
                        a_rng = entity_range::merge(a_rng, b_rng);
                        return true;
                    } else {
                        return false;
                    }
                });
                combine_erase(inits, [](entity_init& a, entity_init const& b) {
                    auto a_rng = std::get<0>(a);
                    auto const b_rng = std::get<0>(b);

                    #ifdef __clang__
                    auto a_func = std::get<1>(a);
                    auto b_func = std::get<1>(b);
                    #else
                    auto const a_func = std::get<1>(a);
                    auto const b_func = std::get<1>(b);
                    #endif

                    if (a_rng.can_merge(b_rng) && (a_func.template target<T(entity_id)>() ==  b_func.template target<T(entity_id)>())) {
                        a_rng = entity_range::merge(a_rng, b_rng);
                        return true;
                    } else {
                        return false;
                    }
                });
            } else { // does not contain data
                auto const combiner = [](auto& a, auto const& b) { // entity_data/entity_init
                    auto& [a_rng] = a;
                    auto const& [b_rng] = b;

                    if (a_rng.can_merge(b_rng)) {
                        a_rng = entity_range::merge(a_rng, b_rng);
                        return true;
                    } else {
                        return false;
                    }
                };
                combine_erase(adds, combiner);
                combine_erase(inits, combiner);
            }

            // Add the new entities/components
            std::vector<entity_range> new_ranges;
            auto it_adds = adds.begin();
            auto ranges_it = ranges.cbegin();

            auto const insert_range = [&](auto const it) {
                entity_range const& range = std::get<0>(*it);
                size_t offset = 0;

                // Copy the current ranges while looking for an insertion point
                while (ranges_it != ranges.cend() && (*ranges_it < range)) {
                    if constexpr (!unbound<T>) {
                        // Advance the component offset so it will point
                        // to the correct components when inserting
                        offset += ranges_it->count();
                    }

                    new_ranges.push_back(*ranges_it++);
                }

                // New range must not already exist in the pool
                if (ranges_it != ranges.cend())
                    Expects(false == ranges_it->overlaps(range));

                // Add or merge the new range
                if (!new_ranges.empty() && new_ranges.back().can_merge(range)) {
                    // Merge the new range with the last one in the vector
                    new_ranges.back() = ecs::entity_range::merge(new_ranges.back(), range);
                } else {
                    // Add the new range
                    new_ranges.push_back(range);
                }

                // return the offset
                return offset;
            };

            if constexpr (!detail::unbound<T>) {
                auto it_inits = inits.begin();
                auto component_it = components.cbegin();

                auto const insert_data = [&](size_t offset) {
                    // Add the new data
                    component_it += offset;
                    size_t const range_count = std::get<0>(*it_adds).count();
                    component_it = components.insert(component_it, range_count, std::move(std::get<1>(*it_adds)));
                    component_it = std::next(component_it, range_count);
                };
                auto const insert_init = [&](size_t offset) {
                    // Add the new data
                    component_it += offset;
                    auto const& range = std::get<0>(*it_inits);
                    auto const& init = std::get<1>(*it_inits);
                    for (entity_id const ent : range) {
                        component_it = components.emplace(component_it, init(ent));
                        component_it = std::next(component_it);
                    }
                };

                while (it_adds != adds.end() && it_inits != inits.end()) {
                    if (std::get<0>(*it_adds) < std::get<0>(*it_inits)) {
                        insert_data(insert_range(it_adds));
                        ++it_adds;
                    } else {
                        insert_init(insert_range(it_inits));
                        ++it_inits;
                    }
                }

                while (it_adds != adds.end()) {
                    insert_data(insert_range(it_adds));
                    ++it_adds;
                }
                while (it_inits != inits.end()) {
                    insert_init(insert_range(it_inits));
                    ++it_inits;
                }
            } else {
                // If there is no data, the ranges are always added to 'deferred_adds'
                while (it_adds != adds.end()) {
                    insert_range(it_adds);
                    ++it_adds;
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
            } else {
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
                if (!std::is_sorted(removes.begin(), removes.end()))
                    std::sort(removes.begin(), removes.end());

                // An entity can not have more than one of the same component
                auto const has_duplicate_entities = [](auto const& vec) {
                    return vec.end() != std::adjacent_find(vec.begin(), vec.end());
                };
                Expects(false == has_duplicate_entities(removes));

                // Merge adjacent ranges
                auto const combiner = [](auto& a, auto const& b) {
                    if (a.can_merge(b)) {
                        a = entity_range::merge(a, b);
                        return true;
                    } else {
                        return false;
                    }
                };
                combine_erase(removes, combiner);

                // Remove the components
                if constexpr (!unbound<T>) {
                    // Find the first valid index
                    auto index = find_entity_index(removes.front().first());
                    Expects(index.has_value());
                    auto dest_it = components.begin() + index.value();
                    auto from_it = dest_it + removes.front().count();

                    if (dest_it == components.begin() && from_it == components.end()) {
                        components.clear();
                    } else {
                        // Move components inbetween the ranges
                        for (auto it = removes.cbegin() + 1; it != removes.cend(); ++it) {
                            index = find_entity_index(it->first());

                            auto const last_it = components.begin() + index.value();
                            auto const dist = std::distance(from_it, last_it);
                            from_it = std::move(from_it, last_it, dest_it);
                            dest_it += dist;
                        }

                        // Move rest of components
                        auto const dist = std::distance(from_it, components.end());
                        std::move(from_it, components.end(), dest_it);

                        // Erase the unused space
                        if (dest_it + dist != components.end()) {
                            components.erase(dest_it + dist, components.end());
                        } else {
                            components.erase(dest_it, components.end());
                        }
                    }
                }

                // Remove the ranges
                auto curr_range = ranges.begin();
                for (auto const& remove : removes) {
                    // Step forward until a candidate range is found
                    while (*curr_range < remove && curr_range != ranges.end()) {
                        ++curr_range;
                    }

                    if (curr_range == ranges.end()) {
                        break;
                    }

                    Expects(curr_range->contains(remove));

                    // Erase the current range if it equals the range to be removed
                    if (curr_range->equals(remove)) {
                        curr_range = ranges.erase(curr_range);
                    } else {
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
} // namespace ecs::detail

#endif // !ECS_COMPONENT_POOL
#ifndef ECS_OPTIONS_H
#define ECS_OPTIONS_H

namespace ecs::opts {
    template<int I>
    struct group {
        static constexpr int group_id = I;
    };

    template<size_t I>
    struct frequency {
        static constexpr size_t hz = I;
    };

    struct manual_update {};

    struct not_parallel {};
    //struct not_concurrent {};

} // namespace ecs::opts

#endif // !ECS_OPTIONS_H
#ifndef ECS_DETAIL_OPTIONS_H
#define ECS_DETAIL_OPTIONS_H

#include <execution>

namespace ecs::detail {
    //
    // Check if type is a group
    template<typename T>
    struct is_group {
        static constexpr bool value = false;
    };
    template<typename T>
    requires requires { T::group_id; }
    struct is_group<T> {
        static constexpr bool value = true;
    };

    //
    // Check if type is a frequency
    template<typename T>
    struct is_frequency {
        static constexpr bool value = false;
    };
    template<typename T>
    requires requires { T::hz; }
    struct is_frequency<T> {
        static constexpr bool value = true;
    };

    // Contains detectors for the options
    namespace detect {
        // A detector that applies Tester to each option.
        template<template<class O> class Tester, class TupleOptions, class NotFoundType = void>
        constexpr auto test_option() {
            if constexpr (std::tuple_size_v<TupleOptions> == 0) {
                return (NotFoundType*) 0;
            } else {
                auto constexpr option_index_finder = [](auto... options) -> int {
                    int index = -1;
                    int counter = 0;

                    (..., [&](auto opt) mutable {
                        if (index == -1 && Tester<decltype(opt)>::value)
                            index = counter;
                        else
                            counter += 1;
                    }(options));

                    return index;
                };

                constexpr int option_index = std::apply(option_index_finder, TupleOptions{});
                if constexpr (option_index != -1) {
                    using opt_type = std::tuple_element_t<option_index, TupleOptions>;
                    return (opt_type*) 0;
                } else {
                    return (NotFoundType*) 0;
                }
            }
        }

        template<class Option, class TupleOptions>
        constexpr bool has_option() {
            if constexpr (std::tuple_size_v<TupleOptions> == 0) {
                return false;
            } else {
                auto constexpr option_index_finder = [](auto... options) -> int {
                    int index = -1;
                    int counter = 0;

                    auto x = [&](auto opt) {
                        if (index == -1 && std::is_same_v<Option, decltype(opt)>)
                            index = counter;
                        else
                            counter += 1;
                    };

                    (..., x(options));

                    return index;
                };

                constexpr int option_index = std::apply(option_index_finder, TupleOptions{});
                return option_index != -1;
            }
        }
    } // namespace detect

    // Use a tester to check the options. Takes a tester structure and a tuple of options to test against.
    // The tester must have static member 'value' that determines if the option passed to it
    // is what it is looking for, see 'is_group' for an example.
    // STL testers like 'std::is_execution_policy' can also be used
    template<template<class O> class Tester, class TupleOptions>
    using test_option_type = std::remove_pointer_t<decltype(detect::test_option<Tester, TupleOptions>())>;

    // Use a tester to check the options. Results in 'NotFoundType' if the tester
    // does not find a viable option.
    template<template<class O> class Tester, class TupleOptions, class NotFoundType>
    using test_option_type_or =
        std::remove_pointer_t<decltype(detect::test_option<Tester, TupleOptions, NotFoundType>())>;

    template<class Option, class TupleOptions>
    constexpr bool has_option() {
        return detect::has_option<Option, TupleOptions>();
    }
} // namespace ecs::detail

#endif // !ECS_DETAIL_OPTIONS_H
#ifndef ECS_FREQLIMIT_H
#define ECS_FREQLIMIT_H

#include <chrono>

namespace ecs::detail {
    // microsecond precision
	template<size_t hz>
	struct frequency_limiter {
        bool can_run() {
            if constexpr (hz == 0)
                return true;
            else {
                using namespace std::chrono_literals;

                auto const now = std::chrono::high_resolution_clock::now();
                auto const diff = now - time;
                if (diff >= (1'000'000'000ns / hz)) {
                    time = now;
                    return true;
                } else {
                    return false;
                }
            }
		}

    private:
        std::chrono::high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now();
    };
}

#endif // !ECS_FREQLIMIT_H
#ifndef ECS_VERIFICATION
#define ECS_VERIFICATION

#include <concepts>
#include <type_traits>


namespace ecs::detail {
    // Given a type T, if it is callable with an entity argument,
    // resolve to the return type of the callable. Otherwise assume the type T.
    template<typename T>
    struct get_type {
        using type = T;
    };

    template<std::invocable<entity_type> T>
    struct get_type<T> {
        using type = std::invoke_result_t<T, entity_type>;
    };

    template<typename T>
    using get_type_t = typename get_type<T>::type;

    // Returns true if all types passed are unique
    template<typename First, typename... T>
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

    template<typename First, typename... T>
    constexpr static bool unique_types_v = unique_types<get_type<First>, get_type_t<T>...>();

    // Ensure that any type in the parameter pack T is only present once.
    template<typename First, typename... T>
    concept unique = unique_types_v<First, T...>;

    template<class T>
    constexpr static bool is_entity = std::is_same_v<std::remove_cvref_t<T>, entity_id>;

    // Implement the requirements for immutable components
    template<typename C>
    constexpr bool req_immutable() {
        // Components flagged as 'immutable' must also be const
        if constexpr (detail::immutable<C>)
            return std::is_const_v<std::remove_reference_t<C>>;
        else
            return true;
    }

    // Implement the requirements for tagged components
    template<typename C>
    constexpr bool req_tagged() {
        // Components flagged as 'tag' must not be references
        if constexpr (detail::tagged<C>)
            return !std::is_reference_v<C> && (sizeof(C) == 1);
        else
            return true;
    }

    // Implement the requirements for shared components
    template<typename C>
    constexpr bool req_shared() {
        // Components flagged as 'share' must not be tags or global
        if constexpr (detail::shared<C>)
            return !detail::tagged<C> && !detail::global<C>;
        else
            return true;
    }

    // Implement the requirements for global components
    template<typename C>
    constexpr bool req_global() {
        // Components flagged as 'global' must not be tags or shared
        // and must not be marked as 'transient'
        if constexpr (detail::global<C>)
            return !detail::tagged<C> && !detail::shared<C> && !detail::transient<C>;
        else
            return true;
    }

    template<class C>
    concept Component = requires {
        requires(req_immutable<C>() && req_tagged<C>() && req_shared<C>() && req_global<C>());
    };


    template<class R, class FirstArg, class... Args>
    concept checked_system = 
            // systems can not return values
            std::is_same_v<R, void> &&

            // systems must take at least one component argument
            (is_entity<FirstArg> ? (sizeof...(Args)) > 0 : true) &&

            // Make sure the first entity is not passed as a reference
            (is_entity<FirstArg> ? !std::is_reference_v<FirstArg> : true) &&

            // Component types can only be specified once
            // requires unique<FirstArg, Args...>; // ICE's gcc 10.1
            unique_types_v<FirstArg, Args...> &&

            // Verify components
            (Component<FirstArg> && (Component<Args> && ...));

    // A small bridge to allow the Lambda concept to activate the system concept
    template<class R, class C, class FirstArg, class... Args>
    requires(checked_system<R, FirstArg, Args...>)
    struct lambda_to_system_bridge {
        lambda_to_system_bridge(R (C::*)(FirstArg, Args...)) {};
        lambda_to_system_bridge(R (C::*)(FirstArg, Args...) const) {};
        lambda_to_system_bridge(R (C::*)(FirstArg, Args...) noexcept) {};
        lambda_to_system_bridge(R (C::*)(FirstArg, Args...) const noexcept) {};
    };

    template<typename T>
    concept lambda = requires {
        // Check all the system requirements
        lambda_to_system_bridge(&T::operator());
    };

    template<class R, class T, class U>
    concept checked_sorter =
        // sorter must return boolean
        std::is_same_v<R, bool> &&

        // Arguments must be of same type
        std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

    // A small bridge to allow the Lambda concept to activate the sorter concept
    template<class R, class C, class T, class U>
    requires(checked_sorter<R, T, U>) struct lambda_to_sorter_bridge {
        lambda_to_sorter_bridge(R (C::*)(T, U)){};
        lambda_to_sorter_bridge(R (C::*)(T, U) const){};
        lambda_to_sorter_bridge(R (C::*)(T, U) noexcept){};
        lambda_to_sorter_bridge(R (C::*)(T, U) const noexcept){};
    };

    template<typename T>
    concept sorter = requires {
        // Check all the sorter requirements
        lambda_to_sorter_bridge(&T::operator());
    };
} // namespace ecs::detail

#endif // !ECS_VERIFICATION
#ifndef ECS_SYSTEM_BASE
#define ECS_SYSTEM_BASE

#include <span>
#include <string>


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

        // Get the signature of the system
        [[nodiscard]] virtual std::string get_signature() const noexcept = 0;

        // Get the hashes of types used by the system with const/reference qualifiers removed
        [[nodiscard]] virtual std::span<detail::type_hash const>
        get_type_hashes() const noexcept = 0;

        // Returns true if this system uses the type
        [[nodiscard]] virtual bool has_component(detail::type_hash hash) const noexcept = 0;

        // Returns true if this system has a dependency on another system
        [[nodiscard]] virtual bool depends_on(system_base const*) const noexcept = 0;

        // Returns true if this system writes data to any component
        [[nodiscard]] virtual bool writes_to_any_components() const noexcept = 0;

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
} // namespace ecs

#endif // !ECS_SYSTEM_BASE
#ifndef ECS_SYSTEM
#define ECS_SYSTEM

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>




namespace ecs::detail {

    // Alias for stored pools
    template<class T>
    using pool = component_pool<std::remove_pointer_t<std::remove_cvref_t<T>>>* const;

    // Get a component pool from a component pool tuple
    template<typename Component, typename Pools>
    component_pool<Component>& get_pool(Pools const& pools) {
        return *std::get<pool<Component>>(pools);
    }

    // Get an entities component from a component pool
    template<typename Component, typename Pools>
    [[nodiscard]] std::remove_cvref_t<Component>* get_component(entity_id const entity, Pools const& pools) {
        using T = std::remove_cvref_t<Component>;

        if constexpr (std::is_pointer_v<T>) {
            static_cast<void>(entity);
            return nullptr;
        } else if constexpr (tagged<T>) {
            static char dummy_arr[sizeof(T)];
            return reinterpret_cast<T*>(dummy_arr);
        } else if constexpr (shared<T> || global<T>) {
            return &get_pool<T>(pools).get_shared_component();
        } else {
            return get_pool<T>(pools).find_component_data(entity);
        }
    }

    // Extracts a component argument from a tuple
    template<typename Component, typename Tuple>
    decltype(auto) extract_arg(Tuple const& tuple, [[maybe_unused]] ptrdiff_t offset) {
        using T = std::remove_cvref_t<Component>;
        if constexpr (std::is_pointer_v<T>) {
            return nullptr;
        } else if constexpr (detail::unbound<T>) {
            T* ptr = std::get<T*>(tuple);
            return *ptr;
        } else {
            T* ptr = std::get<T*>(tuple);
            return *(ptr + offset);
        }
    }

    // Gets the type a sorting functions operates on.
    // Has to be outside of system or clang craps itself
    template<class R, class C, class T1, class T2>
    struct get_sort_func_type_impl {
        explicit get_sort_func_type_impl(R (C::*)(T1, T2) const) {
        }

        using type = std::remove_cvref_t<T1>;
    };

    // Holds a pointer to the first component from each pool
    template<class FirstComponent, class... Components>
    using argument_tuple =
        std::conditional_t<is_entity<FirstComponent>, std::tuple<std::remove_cvref_t<Components>*...>,
            std::tuple<std::remove_cvref_t<FirstComponent>*, std::remove_cvref_t<Components>*...>>;

    // Tuple holding component pools
    template<class FirstComponent, class... Components>
    using tup_pools = std::conditional_t<is_entity<FirstComponent>, std::tuple<pool<Components>...>,
        std::tuple<pool<FirstComponent>, pool<Components>...>>;

    // Manages arguments using ranges. Very fast linear traversal and minimal storage overhead.
    template<class Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    struct ranged_argument_builder {
        // Determine the execution policy from the options (or lack thereof)
        using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(),
            std::execution::sequenced_policy, std::execution::parallel_policy>;

        ranged_argument_builder(
            UpdateFn update_func, SortFn /*sort*/, pool<FirstComponent> first_pool, pool<Components>... pools)
            : pools{first_pool, pools...}
            , update_func{update_func} {
        }

        ranged_argument_builder(UpdateFn update_func, SortFn /*sort*/, pool<Components>... pools)
            : pools{pools...}
            , update_func{update_func} {
        }

        tup_pools<FirstComponent, Components...> get_pools() const {
            return pools;
        }

        void run() {
            // Call the system for all the components that match the system signature
            for (auto const& argument : arguments) {
                auto const& range = std::get<entity_range>(argument);
                auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
                std::for_each(e_p, range.begin(), range.end(), [this, &argument, first_id = range.first()](auto ent) {
                    auto const offset = ent - first_id;
                    if constexpr (is_entity<FirstComponent>) {
                        update_func(ent, extract_arg<Components>(argument, offset)...);
                    } else {
                        update_func(extract_arg<FirstComponent>(argument, offset),
                            extract_arg<Components>(argument, offset)...);
                    }
                });
            }
        }

        // Convert a set of entities into arguments that can be passed to the system
        void build(entity_range_view entities) {
            // Build the arguments for the ranges
            arguments.clear();
            for (auto const& range : entities) {
                if constexpr (is_entity<FirstComponent>) {
                    arguments.emplace_back(range, get_component<Components>(range.first(), pools)...);
                } else {
                    arguments.emplace_back(range, get_component<FirstComponent>(range.first(), pools),
                        get_component<Components>(range.first(), pools)...);
                }
            }
        }

    private:
        // Holds an entity range and its arguments
        using range_argument =
            decltype(std::tuple_cat(std::tuple<entity_range>{{0, 1}}, argument_tuple<FirstComponent, Components...>{}));

        // A tuple of the fully typed component pools used by this system
        tup_pools<FirstComponent, Components...> const pools;

        // The user supplied system
        UpdateFn update_func;

        // Holds the arguments for a range of entities
        std::vector<range_argument> arguments;
    };

    // Manages sorted arguments. Neither cache- nor storage space friendly, but arguments
    // will be passed to the user supplied lambda in a sorted manner
    template<typename Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    struct sorted_argument_builder {
        // Determine the execution policy from the options (or lack thereof)
        using execution_policy = std::conditional_t<ecs::detail::has_option<opts::not_parallel, Options>(),
            std::execution::sequenced_policy, std::execution::parallel_policy>;

        sorted_argument_builder(
            UpdateFn update_func, SortFn sort, pool<FirstComponent> first_pool, pool<Components>... pools)
            : pools{first_pool, pools...}
            , update_func{update_func}
            , sort_func{sort} {
        }

        sorted_argument_builder(UpdateFn update_func, SortFn sort, pool<Components>... pools)
            : pools{pools...}
            , update_func{update_func}
            , sort_func{sort} {
        }

        tup_pools<FirstComponent, Components...> get_pools() const {
            return pools;
        }

        void run() {
            // Sort the arguments if the component data has been modified
            if (needs_sorting || std::get<pool<sort_type>>(pools)->has_components_been_modified()) {
                auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
                std::sort(e_p, arguments.begin(), arguments.end(), [this](auto const& l, auto const& r) {
                    sort_type* t_l = std::get<sort_type*>(l);
                    sort_type* t_r = std::get<sort_type*>(r);
                    return sort_func(*t_l, *t_r);
                });

                needs_sorting = false;
            }

            auto const e_p = execution_policy{}; // cannot pass 'execution_policy{}' directly to for_each in gcc
            std::for_each(e_p, arguments.begin(), arguments.end(), [this](auto packed_arg) {
                if constexpr (is_entity<FirstComponent>) {
                    update_func(std::get<0>(packed_arg), extract_arg<Components>(packed_arg, 0)...);
                } else {
                    update_func(extract_arg<FirstComponent>(packed_arg, 0), extract_arg<Components>(packed_arg, 0)...);
                }
            });
        }

        // Convert a set of entities into arguments that can be passed to the system
        void build(entity_range_view entities) {
            if (entities.size() == 0) {
                arguments.clear();
                return;
            }

            // Count the total number of arguments
            size_t arg_count = 0;
            for (auto const& range : entities) {
                arg_count += range.count();
            }

            // Reserve space for the arguments
            arguments.clear();
            arguments.reserve(arg_count);

            // Build the arguments for the ranges
            for (auto const& range : entities) {
                for (entity_id const& entity : range) {
                    if constexpr (is_entity<FirstComponent>) {
                        arguments.emplace_back(entity, get_component<Components>(entity, pools)...);
                    } else {
                        arguments.emplace_back(entity, get_component<FirstComponent>(entity, pools),
                            get_component<Components>(entity, pools)...);
                    }
                }
            }

            needs_sorting = true;
        }

    private:
        // Holds a single entity id and its arguments
        using single_argument =
            decltype(std::tuple_cat(std::tuple<entity_id>{0}, argument_tuple<FirstComponent, Components...>{}));

        using sort_type = typename decltype(get_sort_func_type_impl(&SortFn::operator()))::type;
        static_assert(std::predicate<SortFn, sort_type, sort_type>, "Sorting function is not a predicate");

        // A tuple of the fully typed component pools used by this system
        tup_pools<FirstComponent, Components...> const pools;

        // The user supplied system
        UpdateFn update_func;

        // The user supplied sorting function
        SortFn sort_func;

        // The vector of unrolled arguments, sorted using 'sort_func'
        std::vector<single_argument> arguments;

        // True if the data needs to be sorted
        bool needs_sorting = false;
    };

    // Chooses an argument builder and returns a nullptr to it
    template<typename Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    constexpr auto get_ptr_builder() {
        if constexpr (!std::is_same_v<SortFn, std::nullptr_t>) {
            return (sorted_argument_builder<Options, UpdateFn, SortFn, FirstComponent, Components...>*) nullptr;
        } else {
            return (ranged_argument_builder<Options, UpdateFn, SortFn, FirstComponent, Components...>*) nullptr;
        }
    }

    template<typename Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    using builder_selector =
        std::remove_pointer_t<decltype(get_ptr_builder<Options, UpdateFn, SortFn, FirstComponent, Components...>())>;

    // The implementation of a system specialized on its components
    template<typename Options, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    class system final : public system_base {
        using argument_builder = builder_selector<Options, UpdateFn, SortFn, FirstComponent, Components...>;
        argument_builder arguments;

        using user_freq = test_option_type_or<is_frequency, Options, opts::frequency<0>>;
        frequency_limiter<user_freq::hz> frequency;

    public:
        // Constructor for when the first argument to the system is _not_ an entity
        system(UpdateFn update_func, SortFn sort_func, pool<FirstComponent> first_pool, pool<Components>... pools)
            : arguments{update_func, sort_func, first_pool, pools...} {
            find_entities();
        }

        // Constructor for when the first argument to the system _is_ an entity
        system(UpdateFn update_func, SortFn sort_func, pool<Components>... pools)
            : arguments{update_func, sort_func, pools...} {
            find_entities();
        }

        void run() override {
            if (!is_enabled()) {
                return;
            }

            if (!frequency.can_run()) {
                return;
            }

            arguments.run();

            // Notify pools if data was written to them
            if constexpr (!is_entity<FirstComponent>) {
                notify_pool_modifed<FirstComponent>();
            }
            (notify_pool_modifed<Components>(), ...);
        }

        template<typename T>
        void notify_pool_modifed() {
            if constexpr (!is_read_only<T>() && !std::is_pointer_v<T>) {
                get_pool<std::remove_cvref_t<T>>().notify_components_modified();
            }
        }

        constexpr int get_group() const noexcept override {
            using group = test_option_type_or<is_group, Options, opts::group<0>>;
            return group::group_id;
        }

        std::string get_signature() const noexcept override {
            // Component names
            constexpr std::array<std::string_view, num_arguments> argument_names{
                get_type_name<FirstComponent>(), get_type_name<Components>()...};

            std::string sig("system(");
            for (size_t i = 0; i < num_arguments - 1; i++) {
                sig += argument_names[i];
                sig += ", ";
            }
            sig += argument_names[num_arguments - 1];
            sig += ')';
            return sig;
        }

        constexpr std::span<detail::type_hash const> get_type_hashes() const noexcept override {
            return type_hashes;
        }

        constexpr bool has_component(detail::type_hash hash) const noexcept override {
            return type_hashes.end() != std::find(type_hashes.begin(), type_hashes.end(), hash);
        }

        constexpr bool depends_on(system_base const* other) const noexcept override {
            for (auto hash : get_type_hashes()) {
                // If the other system doesn't touch the same component,
                // then there can be no dependecy
                if (!other->has_component(hash))
                    continue;

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
                        continue;
                    }
                }
            }

            return false;
        }

        constexpr bool writes_to_any_components() const noexcept override {
            if constexpr (!is_entity<FirstComponent> && !std::is_const_v<std::remove_reference_t<FirstComponent>>)
                return true;
            else {
                return ((!std::is_const_v<std::remove_reference_t<Components>>) &&...);
            }
        }

        constexpr bool writes_to_component(detail::type_hash hash) const noexcept override {
            auto const it = std::find(type_hashes.begin(), type_hashes.end(), hash);
            if (it == type_hashes.end())
                return false;

            // Contains true if a type is read-only
            constexpr std::array<bool, num_components> type_read_only =
                get_type_read_only<is_entity<FirstComponent>, FirstComponent, Components...>();

            return !type_read_only[std::distance(type_hashes.begin(), it)];
        }

        template<bool ignore_first_arg, typename First, typename... Types>
        static constexpr auto get_type_read_only() {
            if constexpr (!ignore_first_arg) {
                return std::array<bool, 1 + sizeof...(Types)>{is_read_only<First>(), is_read_only<Types>()...};
            } else {
                return std::array<bool, sizeof...(Types)>{is_read_only<Types>()...};
            }
        }

        template<typename T>
        static constexpr bool is_read_only() {
            return detail::immutable<T> || detail::tagged<T> || std::is_const_v<std::remove_reference_t<T>>;
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

            bool const modified = std::apply(
                [](auto... pools) { return (pools->has_component_count_changed() || ...); }, arguments.get_pools());

            if (modified) {
                find_entities();
            }
        }

        // Locate all the entities affected by this system
        // and send them to the argument builder
        void find_entities() {
            if constexpr (num_components == 1) {
                // Build the arguments
                entity_range_view const entities = std::get<0>(arguments.get_pools())->get_entities();
                arguments.build(entities);
            } else {
                // When there are more than one component required for a system,
                // find the intersection of the sets of entities that have those components

                // The intersector
                std::vector<entity_range> ranges;
                bool first_component = true;
                auto const intersect = [&](auto arg) {
                    using type = std::remove_pointer_t<decltype(arg)>;
                    if constexpr (std::is_pointer_v<type> || detail::global<type>) {
                        // Skip pointers and globals
                        return;
                    } else {
                        auto const& type_pool = get_pool<type>();

                        if (first_component) {
                            entity_range_view const span = type_pool.get_entities();
                            ranges.insert(ranges.end(), span.begin(), span.end());
                            first_component = false;
                        } else {
                            ranges = intersect_ranges(ranges, type_pool.get_entities());
                        }
                    }
                };

                // Find the intersections
                constexpr auto dummy = argument_tuple<FirstComponent, Components...>{};
                std::apply([&intersect](auto... args) { (..., intersect(args)); }, dummy);

                // Filter out types if needed
                if constexpr (num_filters > 0) {
                    auto const difference = [&](auto arg) {
                        using type = std::remove_pointer_t<decltype(arg)>;
                        if constexpr (std::is_pointer_v<type>) {
                            auto const& type_pool = get_pool<std::remove_pointer_t<type>>();
                            ranges = difference_ranges(ranges, type_pool.get_entities());
                        }
                    };

                    if (!ranges.empty())
                        std::apply([&difference](auto... args) { (..., difference(args)); }, dummy);
                }

                // Build the arguments
                arguments.build(ranges);
            }
        }

        template<typename Component>
        [[nodiscard]] component_pool<Component>& get_pool() const {
            return detail::get_pool<Component>(arguments.get_pools());
        }

    private:
        // Number of arguments
        static constexpr size_t num_arguments = 1 + sizeof...(Components);

        // Number of components
        static constexpr size_t num_components = sizeof...(Components) + !is_entity<FirstComponent>;

        // Number of filters
        static constexpr size_t num_filters = (std::is_pointer_v<FirstComponent> + ... + std::is_pointer_v<Components>);
        static_assert(num_filters < num_components, "systems must have at least one non-filter component");

        // Hashes of stripped types used by this system ('int' instead of 'int const&')
        static constexpr std::array<detail::type_hash, num_components> type_hashes =
            get_type_hashes_array<is_entity<FirstComponent>, std::remove_cvref_t<FirstComponent>,
                std::remove_cvref_t<Components>...>();
    };
} // namespace ecs::detail

#endif // !ECS_SYSTEM
#ifndef ECS_SYSTEM_SCHEDULER
#define ECS_SYSTEM_SCHEDULER

#include <algorithm>
#include <atomic>
#include <execution>
#include <vector>


namespace ecs::detail {
    // Describes a node in the scheduler execution graph
    struct scheduler_node final {
        // Construct a node from a system.
        // The system can not be null
        scheduler_node(detail::system_base* sys)
            : sys(sys)
            , dependants{}
            , dependencies{0}
            , unfinished_dependencies{0} {
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
            std::for_each(std::execution::par, dependants.begin(), dependants.end(), [&nodes](size_t node) {
                nodes[node].run(nodes);
            });
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
        int16_t dependencies = 0;
        std::atomic<int16_t> unfinished_dependencies = 0;
    };

    // Schedules systems for concurrent execution based on their components.
    class scheduler final {
        // A group of systems with the same group id
        struct group final {
            int id;
            std::vector<scheduler_node> all_nodes;
            std::vector<std::size_t> entry_nodes{};

            void run(size_t node_index) {
                all_nodes[node_index].run(all_nodes);
            }
        };

        std::vector<group> groups;

    protected:
        group& find_group(int id) {
            // Look for an existing group
            if (!groups.empty()) {
                for (auto& group : groups) {
                    if (group.id == id) {
                        return group;
                    }
                }
            }

            // No group found, so find an insertion point
            auto const insert_point =
                std::upper_bound(groups.begin(), groups.end(), id, [](int id, group const& sg) { return id < sg.id; });

            // Insert the group and return it
            return *groups.insert(insert_point, group{id, {}, {}});
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

        void run() {
            // Reset the execution data
            for (auto& group : groups) {
                for (auto& node : group.all_nodes)
                    node.reset_unfinished_dependencies();
            }

            // Run the groups in succession
            for (auto& group : groups) {
                std::for_each(std::execution::par, group.entry_nodes.begin(), group.entry_nodes.end(),
                    [&group](auto node) { group.run(node); });
            }
        }
    };
} // namespace ecs::detail

#endif // !ECS_SYSTEM_SCHEDULER
#ifndef ECS_CONTEXT
#define ECS_CONTEXT

#include <map>
#include <memory>
#include <shared_mutex>
#include <vector>


namespace ecs::detail {
    // The central class of the ecs implementation. Maintains the state of the system.
    class context final {
        // The values that make up the ecs core.
        std::vector<std::unique_ptr<system_base>> systems;
        std::vector<std::unique_ptr<component_pool_base>> component_pools;
        std::map<type_hash, component_pool_base*> type_pool_lookup;
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
        template<typename T>
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
            sched = scheduler();
            // context::component_pools.clear(); // this will cause an exception in
            // get_component_pool() due to the cache
            for (auto& pool : component_pools) {
                pool->clear();
            }
        }

        // Returns a reference to a components pool.
        // If a pool doesn't exist, one will be created.
        template<typename T>
        auto& get_component_pool() {
            thread_local tls::cache<type_hash, component_pool_base*, get_type_hash<void>()> cache;

            constexpr auto hash = get_type_hash<std::remove_pointer_t<std::remove_cvref_t<T>>>();
            auto pool = cache.get_or(hash, [this](type_hash hash) {
                std::shared_lock component_pool_lock(component_pool_mutex);

                // Look in the pool for the type
                auto const it = type_pool_lookup.find(hash);
                if (it == type_pool_lookup.end()) {
                    // The pool wasn't found so create it.
                    // create_component_pool takes a unique lock, so unlock the
                    // shared lock during its call
                    component_pool_lock.unlock();
                    return create_component_pool<std::remove_pointer_t<std::remove_cvref_t<T>>>();
                }
                else {
                    return it->second;
                }
            });

            return *static_cast<component_pool<std::remove_pointer_t<std::remove_cvref_t<T>>>*>(pool);
        }

        // Const lambda with sort
        template<typename Options, typename UpdateFn, typename SortFn, typename R, typename C,
            typename FirstArg, typename... Args>
        auto& create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstArg, Args...) const) {
            return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(
                update_func, sort_func);
        }

        // Mutable lambda with sort
        template<typename Options, typename UpdateFn, typename SortFn, typename R, typename C,
            typename FirstArg, typename... Args>
        auto& create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(FirstArg, Args...)) {
            return create_system<Options, UpdateFn, SortFn, FirstArg, Args...>(
                update_func, sort_func);
        }

    private:
        template<typename Options, typename UpdateFn, typename SortFn, typename FirstArg,
            typename... Args>
        auto& create_system(UpdateFn update_func, SortFn sort_func) {
            // Make sure we have a valid sort function
            if constexpr (!std::is_same_v<SortFn, std::nullptr_t>) {
                static_assert(detail::sorter<SortFn>, "Invalid sort-function supplied, should be 'bool(T const&, T const&)'");
            }

            // Set up the implementation
            using typed_system = system<Options, UpdateFn, SortFn, FirstArg, Args...>;

            // Create the system instance
            std::unique_ptr<system_base> sys;
            if constexpr (is_entity<FirstArg>) {
                sys = std::make_unique<typed_system>(update_func, sort_func, &get_component_pool<Args>()...);
            } else {
                sys = std::make_unique<typed_system>(
                    update_func, sort_func, &get_component_pool<FirstArg>(), &get_component_pool<Args>()...);
            }

            std::unique_lock lock(mutex);
            systems.push_back(std::move(sys));
            detail::system_base* ptr_system = systems.back().get();
            Ensures(ptr_system != nullptr);

            if constexpr (!has_option<opts::manual_update, Options>())
                sched.insert(ptr_system);

            return *ptr_system;
        }

        // Create a component pool for a new type
        template<typename T>
        component_pool_base* create_component_pool() {
            // Create a new pool if one does not already exist
            if (!has_component_pool<T>()) {
                std::unique_lock component_pool_lock(component_pool_mutex);

                auto pool = std::make_unique<component_pool<T>>();
                constexpr auto hash = get_type_hash<T>();
                type_pool_lookup.emplace(hash, pool.get());
                component_pools.push_back(std::move(pool));
                return component_pools.back().get();
            } else
                return &get_component_pool<T>();
        }
    };

    inline context& get_context() {
        static context ctx;
        return ctx;
    }

    // The global reference to the context
    static inline context& _context = get_context();
} // namespace ecs::detail

#endif // !ECS_CONTEXT
#ifndef ECS_RUNTIME
#define ECS_RUNTIME

#include <concepts>
#include <execution>
#include <type_traits>
#include <utility>


namespace ecs {
    // Add several components to a range of entities. Will not be added until 'commit_changes()' is
    // called. Initializers can be used with the function signature 'T(ecs::entity_id)'
    //   where T is the component type returned by the function.
    // Pre: entity does not already have the component, or have it in queue to be added
    template<typename First, typename... T>
    void add_component(entity_range const range, First&& first_val, T&&... vals) {
        static_assert(detail::unique<First, T...>, "the same component was specified more than once");
        static_assert(!detail::global<First> && (!detail::global<T> && ...), "can not add global components to entities");
        static_assert(!std::is_pointer_v<std::remove_cvref_t<First>> && (!std::is_pointer_v<std::remove_cvref_t<T>> && ...), "can not add pointers to entities; wrap them in a struct");

        auto const adder = []<class Type>(entity_range const range, Type&& val) {
            if constexpr (std::is_invocable_v<Type, entity_id> && !detail::unbound<Type>) {
                // Return type of 'func'
                using ComponentType = decltype(std::declval<Type>()(entity_id{0}));
                static_assert(!std::is_same_v<ComponentType, void>,
                    "Initializer functions must return a component");

                // Add it to the component pool
                detail::component_pool<ComponentType>& pool =
                    detail::_context.get_component_pool<ComponentType>();
                pool.add_init(range, std::forward<Type>(val));
            } else {
                // Add it to the component pool
                if constexpr (std::is_reference_v<Type>) {
                    using DerefT = std::remove_reference_t<Type>;
                    static_assert(std::copyable<DerefT>, "Type must be copyable");

                    detail::component_pool<DerefT>& pool = detail::_context.get_component_pool<DerefT>();
                    pool.add(range, std::forward<DerefT>(val));
                } else {
                    static_assert(std::copyable<Type>, "Type must be copyable");

                    detail::component_pool<Type>& pool = detail::_context.get_component_pool<Type>();
                    pool.add(range, std::forward<Type>(val));
                }
            }
        };

        adder(range, std::forward<First>(first_val));
        (adder(range, std::forward<T>(vals)), ...);
    }

    // Add several components to an entity. Will not be added until 'commit_changes()' is called.
    // Pre: entity does not already have the component, or have it in queue to be added
    template<typename First, typename... T>
    void add_component(entity_id const id, First&& first_val, T&&... vals) {
        add_component(
            entity_range{id, id}, std::forward<First>(first_val), std::forward<T>(vals)...);
    }

    // Removes a component from a range of entities. Will not be removed until 'commit_changes()' is
    // called. Pre: entity has the component
    template<detail::persistent T>
    void remove_component(entity_range const range, T const& = T{}) {
        static_assert(!detail::global<T>, "can not remove or add global components to entities");

        // Remove the entities from the components pool
        detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
        pool.remove_range(range);
    }

    // Removes a component from an entity. Will not be removed until 'commit_changes()' is called.
    // Pre: entity has the component
    template<typename T>
    void remove_component(entity_id const id, T const& = T{}) {
        remove_component<T>({id, id});
    }

    // Returns a shared component.
    template<detail::shared T>
    T& get_shared_component() {
        return detail::_context.get_component_pool<T>().get_shared_component();
    }

    // Returns a global component.
    template<detail::global T>
    T& get_global_component() {
        return detail::_context.get_component_pool<T>().get_shared_component();
    }

    // Returns the component from an entity, or nullptr if the entity is not found
    template<detail::local T>
    T* get_component(entity_id const id) {
        // Get the component pool
        detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
        return pool.find_component_data(id);
    }

    // Returns the components from an entity range, or an empty span if the entities are not found
    // or does not containg the component.
    // The span might be invalidated after a call to 'ecs::commit_changes()'.
    template<detail::local T>
    std::span<T> get_components(entity_range const range) {
        if (!has_component<T>(range))
            return {};

        // Get the component pool
        detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
        return {pool.find_component_data(range.first()), range.count()};
    }

    // Returns the number of active components for a specific type of components
    template<typename T>
    size_t get_component_count() {
        if (!detail::_context.has_component_pool<T>())
            return 0;

        // Get the component pool
        detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
        return pool.num_components();
    }

    // Returns the number of entities that has the component.
    template<typename T>
    size_t get_entity_count() {
        if (!detail::_context.has_component_pool<T>())
            return 0;

        // Get the component pool
        detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
        return pool.num_entities();
    }

    // Return true if an entity contains the component
    template<typename T>
    bool has_component(entity_id const id) {
        if (!detail::_context.has_component_pool<T>())
            return false;

        detail::component_pool<T> const& pool = detail::_context.get_component_pool<T>();
        return pool.has_entity(id);
    }

    // Returns true if all entities in a range has the component.
    template<typename T>
    bool has_component(entity_range const range) {
        if (!detail::_context.has_component_pool<T>())
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

    // Commits all changes and calls the 'update' function on all the systems in the order they were
    // added. Same as calling commit_changes() and run_systems().
    inline void update() {
        commit_changes();
        run_systems();
    }

    // Make a new system
    template<typename... Options, detail::lambda UpdateFn, typename SortFn = std::nullptr_t>
    auto& make_system(UpdateFn update_func, SortFn sort_func = nullptr) {
        using opts = std::tuple<Options...>;
        return detail::_context.create_system<opts, UpdateFn, SortFn>(
            update_func, sort_func, &UpdateFn::operator());
    }
} // namespace ecs

#endif // !ECS_RUNTIME

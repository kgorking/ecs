#ifndef __CONTRACT
#define __CONTRACT

// Contracts. If they are violated, the program is an invalid state, so nuke it from orbit
#define Expects(cond) ((cond) ? static_cast<void>(0) : std::terminate())
#define Ensures(cond) ((cond) ? static_cast<void>(0) : std::terminate())

#endif // !__CONTRACT
#ifndef __ENTITY
#define __ENTITY

#include <concepts>


namespace ecs {
    // A simple helper class for easing the adding and removing of components
    class entity final {
        entity_id ent;

    public:
        constexpr entity(entity_id ent) : ent(ent) {}

        template<std::copyable... Components>
        entity(entity_id ent, Components&&... components) : ent(ent) {
            add<Components...>(std::forward<Components>(components)...);
        }

        template<std::copyable... Components>
        void add(Components&&... components) const {
            add_components(ent, std::forward<Components>(components)...);
        }

        template<std::copyable... Components>
        void add() const {
            add_components(ent, Components{}...);
        }

        template<typename... Components>
        void remove() const {
            (remove_component<Components>(ent), ...);
        }

        template<typename... Component>
        [[nodiscard]] bool has() const {
            return (has_component<Component>(ent) && ...);
        }

        template<typename Component>
        [[nodiscard]] Component& get() const {
            return *get_component<Component>(ent);
        }

        [[nodiscard]] constexpr entity_id get_id() const { return ent; }
    };
} // namespace ecs

#endif // !__ENTITY
#ifndef __ENTITY_ID
#define __ENTITY_ID

namespace ecs {
    using entity_type = int;
    using entity_offset = unsigned int; // must cover the entire entity_type domain

    // A simple struct that is an entity identifier.
    // Use a struct so the typesystem can differentiate
    // between entity ids and regular integers in system arguments
    struct entity_id final {
        // Uninitialized entity ids are not allowed, because they make no sense
        entity_id() = delete;

        constexpr entity_id(entity_type _id) noexcept : id(_id) {}

        constexpr operator entity_type&() noexcept { return id; }
        constexpr operator entity_type() const noexcept { return id; }

    private:
        entity_type id;
    };
} // namespace ecs

#endif // !__ENTITY_ID
#ifndef __ENTITY_RANGE
#define __ENTITY_RANGE

#include <algorithm>
#include <limits>
#include <optional>
#include <span>


namespace ecs {
    // Defines a range of entities.
    // 'last' is included in the range.
    class entity_range final {
        entity_id first_;
        entity_id last_;

    public:
        entity_range() = delete; // no such thing as a 'default' range

        constexpr entity_range(entity_id first, entity_id last) : first_(first), last_(last) {
            Expects(first <= last);
        }

        // Construct an entity range and add components to them.
        template<typename... Components>
        entity_range(entity_id first, entity_id last, Components&&... components)
            : first_(first), last_(last) {
            Expects(first <= last);
            add<Components...>(std::forward<Components>(components)...);
        }

        template<typename... Components>
        void add(Components&&... components) const {
            add_components(*this, std::forward<Components>(components)...);
        }

        template<typename... Components>
        void add() const {
            add_components(*this, Components{}...);
        }

        template<std::copyable... Components>
        void remove() const {
            (remove_component<Components>(*this), ...);
        }

        template<std::copyable... Components>
        [[nodiscard]] bool has() const {
            return (has_component<Components>(*this) && ...);
        }

        template<std::copyable Component>
        [[nodiscard]] std::span<Component> get() const {
            return std::span(get_component<Component>(first_), count());
        }

        [[nodiscard]] constexpr entity_iterator begin() const { return entity_iterator{first_}; }

        [[nodiscard]] constexpr entity_iterator end() const { return entity_iterator{last_} + 1; }

        [[nodiscard]] constexpr bool operator==(entity_range const& other) const {
            return equals(other);
        }

        // For sort
        [[nodiscard]] constexpr bool operator<(entity_range const& other) const {
            return first_ < other.first() && last_ < other.last();
        }

        // Returns the first entity in the range
        [[nodiscard]] constexpr entity_id first() const { return first_; }

        // Returns the last entity in the range
        [[nodiscard]] constexpr entity_id last() const { return last_; }

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
        // Pre: 'other' must overlap 'range', but must not be equal to it
        [[nodiscard]] constexpr static std::pair<entity_range, std::optional<entity_range>>
        remove(entity_range const& range, entity_range const& other) {
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
        [[nodiscard]] constexpr static entity_range merge(entity_range const& r1,
                                                          entity_range const& r2) {
            Expects(r1.can_merge(r2));
            return entity_range{r1.first(), r2.last()};
        }

        // Returns the intersection of two ranges
        // Pre: The ranges must overlap, the resulting ranges can not have zero-length
        [[nodiscard]] constexpr static entity_range intersect(entity_range const& range,
                                                              entity_range const& other) {
            Expects(range.overlaps(other));

            entity_id const first{std::max(range.first(), other.first())};
            entity_id const last{std::min(range.last(), other.last())};

            return entity_range{first, last};
        }
    };

    using entity_range_view = std::span<entity_range const>;
} // namespace ecs

#endif // !__ENTITTY_RANGE
#ifndef __TLS_SPLITTER_H
#define __TLS_SPLITTER_H

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

#endif // !__TLS_SPLITTER_H
#ifndef __COMPONENT_SPECIFIER
#define __COMPONENT_SPECIFIER

#include <type_traits>

namespace ecs {
    // Add this in 'ecs_flags()' to mark a component as a tag.
    // Uses O(1) memory instead of O(n).
    // Mutually exclusive with 'share' and 'global'
    struct tag {};

    // Add this in 'ecs_flags()' to mark a component as shared between components,
    // meaning that any entity with a shared component will all point to the same component.
    // Think of it as a static member variable in a regular class.
    // Uses O(1) memory instead of O(n).
    // Mutually exclusive with 'tag' and 'global'
    struct share {};

    // Add this in 'ecs_flags()' to mark a component as transient.
    // The component will only exist on an entity for one cycle,
    // and then be automatically removed.
    // Mutually exclusive with 'global'
    struct transient {};

    // Add this in 'ecs_flags()' to mark a component as constant.
    // A compile-time error will be raised if a system tries to
    // access the component through a non-const reference.
    struct immutable {};

    // Add this is 'ecs_flags()' to mark a component as global.
    // Global components can be referenced from systems without
    // having been added to any entities.
    // Uses O(1) memory instead of O(n).
    // Mutually exclusive with 'tag', 'share', and 'transient'
    struct global {};

// Add flags to a component to change its behaviour and memory usage.
// Example:
// struct my_component {
// 	ecs_flags(ecs::tag, ecs::transient);
// 	// component data
// };
#define ecs_flags(...)                                                                                                 \
    struct _ecs_flags : __VA_ARGS__ {};

    // Some helpers
    namespace detail {
        template<typename T>
        using flags = typename std::remove_cvref_t<T>::_ecs_flags;

        template<typename T>
        concept tagged = std::is_base_of_v<ecs::tag, flags<T>>;

        template<typename T>
        concept shared = std::is_base_of_v<ecs::share, flags<T>>;

        template<typename T>
        concept transient = std::is_base_of_v<ecs::transient, flags<T>>;

        template<typename T>
        concept immutable = std::is_base_of_v<ecs::immutable, flags<T>>;

        template<typename T>
        concept global = std::is_base_of_v<ecs::global, flags<T>>;

        template<typename T>
        concept local = !global<T>;

        template<typename T>
        concept persistent = !transient<T>;

        template<typename T>
        concept unbound = (shared<T> || tagged<T> ||
                           global<T>); // component is not bound to a specific entity (ie static)
    }                                  // namespace detail
} // namespace ecs

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
        component_pool_base& operator=(component_pool_base const&) = delete;
        component_pool_base& operator=(component_pool_base&&) = delete;
        virtual ~component_pool_base() = default;

        virtual void process_changes() = 0;
        virtual void clear_flags() = 0;
        virtual void clear() = 0;
    };
} // namespace ecs::detail

#endif // !__COMPONENT_POOL_BASE
#ifndef __COMPONENT_POOL
#define __COMPONENT_POOL

#include <concepts>
#include <functional>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include <tls/splitter.h>


namespace ecs::detail {
    // For std::visit
    template<class... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };
    template<class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;

    template<std::copyable T>
    class component_pool final : public component_pool_base {
    private:
        // The components
        std::vector<T> components;

        // The entities that have components in this storage.
        std::vector<entity_range> ranges;

        // Keep track of which components to add/remove each cycle
        using variant = std::variant<T, std::function<T(entity_id)>>;
        using entity_data = std::conditional_t<unbound<T>, std::tuple<entity_range>, std::tuple<entity_range, variant>>;
        tls::splitter<std::vector<entity_data>, component_pool<T>> deferred_adds;
        tls::splitter<std::vector<entity_range>, component_pool<T>> deferred_removes;

        // Status flags
        bool data_added = false;
        bool data_removed = false;

    public:
        // Add a component to a range of entities, initialized by the supplied user function
        // Pre: entities has not already been added, or is in queue to be added
        //      This condition will not be checked until 'process_changes' is called.
        template<typename Fn>
        requires(!unbound<T>) void add_init(entity_range const range, Fn&& init) {
            // Add the range and function to a temp storage
            deferred_adds.local().emplace_back(range, std::forward<Fn>(init));
        }

        // Add a component to a range of entity.
        // Pre: entities has not already been added, or is in queue to be added
        //      This condition will not be checked until 'process_changes' is called.
        void add(entity_range const range, T&& component) requires(!global<T>) {
            if constexpr (shared<T> || tagged<T>) {
                deferred_adds.local().push_back(range);
            } else {
                deferred_adds.local().emplace_back(range, std::forward<T>(component));
            }
        }

        // Add a component to an entity.
        // Pre: entity has not already been added, or is in queue to be added.
        //      This condition will not be checked until 'process_changes' is called.
        void add(entity_id const id, T&& component) requires(!global<T>) {
            add({id, id}, std::forward<T>(component));
        }

        // Return the shared component
        T& get_shared_component() requires unbound<T> {
            static T t{};
            return t;
        }

        // Remove an entity from the component pool. This logically removes the component from the
        // entity.
        void remove(entity_id const id) requires(!global<T>) {
            remove_range({id, id});
        }

        // Remove an entity from the component pool. This logically removes the component from the
        // entity.
        void remove_range(entity_range const range) requires(!global<T>) {
            if (!has_entity(range)) {
                return;
            }

            auto& rem = deferred_removes.local();
            if (!rem.empty() && rem.back().can_merge(range)) {
                rem.back() = entity_range::merge(rem.back(), range);
            } else {
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
            if constexpr (detail::global<T>) {
                // globals are accessible to all entities
                static constexpr entity_range global_range{
                    std::numeric_limits<ecs::entity_type>::min(), std::numeric_limits<ecs::entity_type>::max()};
                return entity_range_view{&global_range, 1};
            } else {
                return ranges;
            }
        }

        // Returns true if an entity has a component in this pool
        bool has_entity(entity_id const id) const requires(!global<T>) {
            return has_entity({id, id});
        }

        // Returns true if an entity range has components in this pool
        bool has_entity(entity_range const& range) const requires(!global<T>) {
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
        bool is_queued_add(entity_id const id) requires(!global<T>) {
            return is_queued_add({id, id});
        }

        // Checks the current threads queue for the entity
        bool is_queued_add(entity_range const& range) requires(!global<T>) {
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
        bool is_queued_remove(entity_id const id) requires(!global<T>) {
            return is_queued_remove({id, id});
        }

        // Checks the current threads queue for the entity
        bool is_queued_remove(entity_range const& range) requires(!global<T>) {
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
            // Comparator used for sorting
            auto constexpr comparator = [](entity_data const& l, entity_data const& r) {
                return std::get<0>(l).first() < std::get<0>(r).first();
            };

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
            /*if (!std::is_sorted(adds.begin(), adds.end(), comparator))*/ {
                std::sort(std::execution::par, adds.begin(), adds.end(), comparator);
            }

            // Check the 'add*' functions precondition.
            // An entity can not have more than one of the same component
            auto const has_duplicate_entities = [](auto const& vec) {
                return vec.end() != std::adjacent_find(vec.begin(), vec.end(),
                                        [](auto const& l, auto const& r) { return std::get<0>(l) == std::get<0>(r); });
            };
            Expects(false == has_duplicate_entities(adds));

            // Small helper function for combining ranges
            auto const add_range = [](std::vector<entity_range>& dest, entity_range const& range) {
                // Merge the range or add it
                if (!dest.empty() && dest.back().can_merge(range)) {
                    dest.back() = entity_range::merge(dest.back(), range);
                } else {
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
                        // Advance the component iterator so it will point to the correct components
                        // when i start inserting it
                        component_it += ranges_it->count();
                    }

                    add_range(new_ranges, *ranges_it++);
                }

                // New range must not already exist in the pool
                if (ranges_it != ranges.cend())
                    Expects(false == ranges_it->overlaps(range));

                // Add the new range
                add_range(new_ranges, range);

                if constexpr (!detail::unbound<T>) {
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
                    std::visit(overloaded{add_val, add_init}, std::move(std::get<1>(add)));
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
                    } else {
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
}; // namespace ecs::detail

#endif // !__COMPONENT_POOL
#ifndef __SYSTEM_VERIFICATION
#define __SYSTEM_VERIFICATION

#include <concepts>
#include <type_traits>


namespace ecs::detail {
    // Given a type T, if it is callable with an entity argument,
    // resolve to the return type of the callable. Otherwise assume the type T.
    template<typename T>
    struct get_type {
        using type = T;
    };

    template<std::invocable<int> T>
    struct get_type<T> {
        using type = std::invoke_result_t<T, int>;
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

    template<typename... T>
    constexpr static bool unique_types_v = unique_types<get_type_t<T>...>();

    // Ensure that any type in the parameter pack T is only present once.
    template<typename... T>
    concept unique = unique_types_v<T...>;

    template<class T>
    concept entity_type =
        std::is_same_v<std::remove_cvref_t<T>, entity_id> || std::is_same_v<std::remove_cvref_t<T>, entity>;

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
    concept checked_system = requires {
        // systems can not return values
        requires std::is_same_v<R, void>;

        // no pointers allowed
        // requires !std::is_pointer_v<FirstArg> && (!std::is_pointer_v<Args> && ...);

        // systems must take at least one component argument
        requires(entity_type<FirstArg> ? sizeof...(Args) >= 1 : true);

        // Make sure the first entity is not passed as a reference
        requires(entity_type<FirstArg> ? !std::is_reference_v<FirstArg> : true);

        // Component types can only be specified once
        requires unique<FirstArg, Args...>;

        // Verify components
        requires Component<FirstArg> && (Component<Args> && ...);
    };

    // A small bridge to allow the Lambda concept to activate the system concept
    template<class R, class C, class... Args>
    requires(sizeof...(Args) > 0 && checked_system<R, Args...>) struct lambda_to_system_bridge {
        lambda_to_system_bridge(R (C::*)(Args...)){};
        lambda_to_system_bridge(R (C::*)(Args...) const){};
        lambda_to_system_bridge(R (C::*)(Args...) noexcept){};
        lambda_to_system_bridge(R (C::*)(Args...) const noexcept){};
    };

    template<typename T>
    concept lambda = requires {
        // Must have the call operator
        &T::operator();

        // Check all the system requirements
        lambda_to_system_bridge(&T::operator());
    };

    template<class R, class T, class U>
    concept checked_sorter = requires {
        // sorter must return boolean
        requires std::is_same_v<R, bool>;

        // Arguments must be of same type
        requires std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

        // Most obey strict ordering
        requires std::totally_ordered_with<T, U>;
    };

    // A small bridge to allow the Lambda concept to activate the sorter concept
    template<class R, class C, class... Args>
    requires(sizeof...(Args) == 2 && checked_sorter<R, Args...>) struct lambda_to_sorter_bridge {
        lambda_to_sorter_bridge(R (C::*)(Args...)){};
        lambda_to_sorter_bridge(R (C::*)(Args...) const){};
        lambda_to_sorter_bridge(R (C::*)(Args...) noexcept){};
        lambda_to_sorter_bridge(R (C::*)(Args...) const noexcept){};
    };

    template<typename T>
    concept sorter = requires {
        // Must have the call operator
        &T::operator();

        // Check all the sorter requirements
        lambda_to_sorter_bridge(&T::operator());
    };
} // namespace ecs::detail

#endif // !__SYSTEM_VERIFICATION
#ifndef __SYSTEM_BASE
#define __SYSTEM_BASE

#include <span>
#include <string>


namespace ecs::detail {
    class context;
}

namespace ecs {
    class system_base {
    public:
        system_base() = default;
        virtual ~system_base() = default;
        system_base(system_base const&) = delete;
        system_base(system_base&&) = default;
        system_base& operator=(system_base const&) = delete;
        system_base& operator=(system_base&&) = default;

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

#endif // !__SYSTEM_BASE
#ifndef __SYSTEM
#define __SYSTEM

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>


namespace ecs::detail {
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

    template<typename T>
    constexpr bool is_read_only() {
        return detail::immutable<T> || detail::tagged<T> || std::is_const_v<std::remove_reference_t<T>>;
    }

    template<bool ignore_first_arg, typename First, typename... Types>
    constexpr auto get_type_read_only() {
        if constexpr (!ignore_first_arg) {
            std::array<bool, 1 + sizeof...(Types)> arr{is_read_only<First>(), is_read_only<Types>()...};
            return arr;
        } else {
            std::array<bool, sizeof...(Types)> arr{is_read_only<Types>()...};
            return arr;
        }
    }

    // Gets the type a sorting functions operates on
    template<class R, class C, class T1, class T2>
    struct get_sort_func_type_impl {
        get_sort_func_type_impl(R (C::*)(T1, T2) const) {}
        using type = std::remove_cvref_t<T1>;
    };
    template<class F>
    using sort_func_type = typename decltype(get_sort_func_type_impl(&F::operator()))::type;


    // The implementation of a system specialized on its components
    template<int Group, class ExePolicy, typename UpdateFn, typename SortFn, class FirstComponent, class... Components>
    class system final : public system_base {

        // Determines if the first component is an entity
        static constexpr bool is_first_arg_entity =
            std::is_same_v<FirstComponent, entity_id> || std::is_same_v<FirstComponent, entity>;

        // Number of arguments
        static constexpr size_t num_arguments = 1 + sizeof...(Components);

        // Calculate the number of components
        static constexpr size_t num_components = sizeof...(Components) + (is_first_arg_entity ? 0 : 1);

        // Calculate the number of filters
        static constexpr size_t num_filters = (std::is_pointer_v<FirstComponent> + ... + std::is_pointer_v<Components>);
        static_assert(num_filters < num_components, "systems must have at least one non-filter component");

        // Component names
        static constexpr std::array<std::string_view, num_arguments> argument_names{
            get_type_name<FirstComponent>(), get_type_name<Components>()...};

        // Hashes of stripped types used by this system ('int' instead of 'int const&')
        static constexpr std::array<detail::type_hash, num_components> type_hashes =
            get_type_hashes_array<is_first_arg_entity, std::remove_cvref_t<FirstComponent>, std::remove_cvref_t<Components>...>();

        // Contains true if a type is read-only
        static constexpr std::array<bool, num_components> type_read_only =
            get_type_read_only<is_first_arg_entity, FirstComponent, Components...>();

        // Alias for stored pools
        template<class T>
        using pool = component_pool<std::remove_pointer_t<std::remove_cvref_t<T>>>* const;

        // Tuple holding all pools used by this system
        using tup_pools = std::conditional_t<is_first_arg_entity, std::tuple<pool<Components>...>,
            std::tuple<pool<FirstComponent>, pool<Components>...>>;

        // Holds a pointer to the first component from each pool
        using argument_tuple = std::conditional_t<is_first_arg_entity, std::tuple<std::remove_cvref_t<Components>*...>,
            std::tuple<std::remove_cvref_t<FirstComponent>*, std::remove_cvref_t<Components>*...>>;

        // Holds an entity range and its arguments
        using range_argument = decltype(std::tuple_cat(std::tuple<entity_range>{{0, 1}}, argument_tuple{}));

        // Holds a single entity id and its arguments
        using packed_argument = decltype(std::tuple_cat(std::tuple<entity_id>{0}, argument_tuple{}));

        // Holds the arguments for a range of entities
        std::vector<range_argument> arguments;

        // A tuple of the fully typed component pools used by this system
        tup_pools const pools;

        // The user supplied system
        UpdateFn update_func;

        // True if a valid sorting function is supplied
        static constexpr bool has_sort_func = !std::is_same_v<std::nullptr_t, SortFn>;

        // The user supplied sorting function
        SortFn sort_func;

        // The vector of unrolled arguments, sorted using 'sort_func'
        std::vector<packed_argument> sorted_arguments;

    public:
        // Constructor for when the first argument to the system is _not_ an entity
        system(UpdateFn update_func, SortFn sort_func, pool<FirstComponent> first_pool, pool<Components>... pools)
            : pools{first_pool, pools...}, update_func{update_func}, sort_func{sort_func} {
            build_args();
        }

        // Constructor for when the first argument to the system _is_ an entity
        system(UpdateFn update_func, SortFn sort_func, pool<Components>... pools)
            : pools{pools...}, update_func{update_func}, sort_func{sort_func} {
            build_args();
        }

        void update() override {
            if (!is_enabled()) {
                return;
            }

            if constexpr (has_sort_func) {
                using sort_type = sort_func_type<SortFn>;
                static_assert(std::predicate<SortFn, sort_type, sort_type>);

                // Sort the arguments
                // if get_pool is_data_modified
                std::sort(sorted_arguments.begin(), sorted_arguments.end(), [this](auto const& l, auto const& r) {
                    sort_type* t_l = std::get<sort_type*>(l);
                    sort_type* t_r = std::get<sort_type*>(r);
                    return sort_func(*t_l, *t_r);
                });

                std::for_each(ExePolicy{}, sorted_arguments.begin(), sorted_arguments.end(), [this](auto packed_arg) {
                    if constexpr (is_first_arg_entity) {
                        update_func(std::get<0>(packed_arg), *std::get<std::remove_cvref_t<Components>*>(packed_arg)...);
                    } else {
                        update_func(
                            *std::get<std::remove_cvref_t<FirstComponent>*>(packed_arg), *std::get<std::remove_cvref_t<Components>*>(packed_arg)...);
                    }
                });
            } else {
                // Small helper function
                auto const extract_arg = [](auto ptr, [[maybe_unused]] ptrdiff_t offset) -> decltype(auto) {
                    using T = std::remove_cvref_t<decltype(*ptr)>;
                    if constexpr (std::is_pointer_v<T>) {
                        return nullptr;
                    } else if constexpr (detail::unbound<T>) {
                        return *ptr;
                    } else {
                        return *(ptr + offset);
                    }
                };

                // Call the system for all pairs of components that match the system signature
                for (auto const& argument : arguments) {
                    auto const& range = std::get<entity_range>(argument);
                    std::for_each(ExePolicy{}, range.begin(), range.end(),
                        [extract_arg, this, &argument, first_id = range.first()](auto ent) {
                            auto const offset = ent - first_id;
                            if constexpr (is_first_arg_entity) {
                                update_func(ent, extract_arg(std::get<std::remove_cvref_t<Components>*>(argument), offset)...);
                            } else {
                                update_func(extract_arg(std::get<std::remove_cvref_t<FirstComponent>*>(argument), offset),
                                    extract_arg(std::get<std::remove_cvref_t<Components>*>(argument), offset)...);
                            }
                        });
                }
            }
        }

        constexpr int get_group() const noexcept override { return Group; }

        std::string get_signature() const noexcept override {
            std::string sig("system(");
            for (size_t i = 0; i < num_arguments - 1; i++) {
                sig += argument_names[i];
                sig += ", ";
            }
            sig += argument_names[num_arguments - 1];
            sig += ')';
            return sig;
        }

        constexpr std::span<detail::type_hash const> get_type_hashes() const noexcept override { return type_hashes; }

        constexpr bool has_component(detail::type_hash hash) const noexcept override {
            return static_has_component(hash);
        }

        static constexpr bool static_has_component(detail::type_hash hash) noexcept {
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
            if constexpr (!is_first_arg_entity && !std::is_const_v<std::remove_reference_t<FirstComponent>>)
                return true;
            else {
                return ((!std::is_const_v<std::remove_reference_t<Components>>) &&...);
            }
        }

        constexpr bool writes_to_component(detail::type_hash hash) const noexcept override {
            auto const it = std::find(type_hashes.begin(), type_hashes.end(), hash);
            if (it == type_hashes.end())
                return false;

            return !type_read_only[std::distance(type_hashes.begin(), it)];
        }

    protected:
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

            auto constexpr is_pools_modified = [](auto... pools) { return (pools->is_data_modified() || ...); };
            bool const is_modified = std::apply(is_pools_modified, pools);

            if (is_modified) {
                build_args();
            }
        }

        void build_args() {
            if constexpr (has_sort_func) {
                // Check that the system has the type that sort_func wants to sort on
                static_assert(static_has_component(get_type_hash<sort_func_type<SortFn>>()),
                    "sorting function operates on a type that the system does not have");
            }

            if constexpr (num_components == 1) {
                // Build the arguments
                entity_range_view const entities = std::get<0>(pools)->get_entities();
                build_args(entities);
            } else {
                // When there are more than one component required for a system,
                // find the intersection of the sets of entities that have those components

                // Intersects two ranges of entities
                auto const intersect_ranges = [](entity_range_view view_a, entity_range_view view_b) {
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
                };

                auto const difference_ranges = [](entity_range_view view_a, entity_range_view view_b) -> std::vector<entity_range> {
                    if (view_a.empty())
                        return {view_b.begin(), view_b.end()};
                    if (view_b.empty())
                        return {view_a.begin(), view_a.end()};

                    std::vector<entity_range> result;
                    auto it_a = view_a.begin();
                    auto it_b = view_b.begin();

                    while (it_a != view_a.end() && it_b != view_b.end()) {
                        if (it_a->overlaps(*it_b) && !it_a->equals(*it_b)) {
                            auto res = entity_range::remove(*it_a, *it_b);
                            result.push_back(res.first);
                            if (res.second.has_value())
                                result.push_back(res.second.value());
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
                };

                // Intersect the entity ranges
                // auto const intersect = do_intersection(entities, get_pool<std::remove_cvref_t<Components>>().get_entities()...);

                // The intersector
                std::optional<std::vector<entity_range>> ranges;
                auto const intersect = [&, this](auto arg) { // arg = std::remove_cvref_t<Components>*
                    using type = std::remove_pointer_t<decltype(arg)>;
                    if constexpr (std::is_pointer_v<type>) {
                        // Skip pointers
                        return;
                    } else {
                        auto const& type_pool = get_pool<type>();
                        // auto& type_pool = *std::get<pool<type>>(pools);

                        if (ranges.has_value()) {
                            ranges = intersect_ranges(*ranges, type_pool.get_entities());
                        } else {
                            auto span = type_pool.get_entities();
                            ranges.emplace(span.begin(), span.end());
                        }
                    }
                };

                auto const difference = [&, this](auto arg) { // arg = std::remove_cvref_t<Components>*
                    using type = std::remove_pointer_t<decltype(arg)>;
                    if constexpr (std::is_pointer_v<type>) {
                        auto& type_pool = get_pool<std::remove_pointer_t<type>>();
                        ranges = difference_ranges(*ranges, type_pool.get_entities());
                    }
                };

                // Find the intersections and differences
                auto dummy = argument_tuple{};
                std::apply([&intersect](auto... args) { (..., intersect(args)); }, dummy);
                std::apply([&difference](auto... args) { (..., difference(args)); }, dummy);

                // Build the arguments
                if (ranges.has_value())
                    build_args(ranges.value());
                else
                    arguments.clear();
            }

            // Unpack the arguments and sort them
            if constexpr (has_sort_func) {
                // Count the total number of arguments
                size_t arg_count = 0;
                for (auto const& args : arguments) { arg_count += std::get<0>(args).count(); }

                // Unpack the arguments
                sorted_arguments.clear();
                sorted_arguments.reserve(arg_count);
                for (auto const& args : arguments) {
                    for (entity_id const& entity : std::get<0>(args)) {
                        if constexpr (is_first_arg_entity) {
                            sorted_arguments.emplace_back(entity, get_component<std::remove_cvref_t<Components>>(entity)...);
                        } else {
                            sorted_arguments.emplace_back(entity, get_component<std::remove_cvref_t<FirstComponent>>(entity),
                                get_component<std::remove_cvref_t<Components>>(entity)...);
                        }
                    }
                }
            }
        }

        // Convert a set of entities into arguments that can be passed to the system
        void build_args(entity_range_view entities) {
            // Build the arguments for the ranges
            arguments.clear();
            for (auto const& range : entities) {
                if constexpr (is_first_arg_entity) {
                    arguments.emplace_back(range, get_component<std::remove_cvref_t<Components>>(range.first())...);
                } else {
                    arguments.emplace_back(range, get_component<std::remove_cvref_t<FirstComponent>>(range.first()),
                        get_component<std::remove_cvref_t<Components>>(range.first())...);
                }
            }
        }

        template<typename Component>
        [[nodiscard]] component_pool<Component>& get_pool() const {
            return *std::get<pool<Component>>(pools);
        }

        template<typename Component>
        [[nodiscard]] Component* get_component(entity_id const entity) {
            if constexpr (std::is_pointer_v<Component>) {
                static_cast<void>(entity);
                static Component* dummy = nullptr;
                return dummy;
            } else {
                return get_pool<Component>().find_component_data(entity);
            }
        }
    }; // namespace ecs::detail
} // namespace ecs::detail

#endif // !__SYSTEM
#ifndef __CONTEXT
#define __CONTEXT

#include <map>
#include <memory>
#include <shared_mutex>
#include <tls/cache.h>
#include <vector>


namespace ecs::detail {
    // The central class of the ecs implementation. Maintains the state of the system.
    class context final {
        // The values that make up the ecs core.
        std::vector<std::unique_ptr<system_base>> systems;
        std::vector<std::unique_ptr<component_pool_base>> component_pools;
        std::map<type_hash, component_pool_base*> type_pool_lookup;
        scheduler sched;

        mutable std::shared_mutex mutex;

    public:
        // Commits the changes to the entities.
        void commit_changes() {
            // Prevent other threads from
            //  adding components
            //  registering new component types
            //  adding new systems
            std::shared_lock lock(mutex);

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
            std::shared_lock lock(mutex);

            // Run all the systems
            sched.run();
        }

        // Returns true if a pool for the type exists
        template<typename T>
        bool has_component_pool() const {
            // Prevent other threads from registering new component types
            std::shared_lock lock(mutex);

            constexpr auto hash = get_type_hash<T>();
            return type_pool_lookup.contains(hash);
        }

        // Resets the runtime state. Removes all systems, empties component pools
        void reset() {
            std::unique_lock lock(mutex);

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

            using NakedType = std::remove_pointer_t<std::remove_cvref_t<T>>;

            constexpr auto hash = get_type_hash<NakedType>();
            auto pool = cache.get_or(hash, [this](type_hash hash) {
                std::shared_lock lock(mutex);

                // Look in the pool for the type
                auto const it = type_pool_lookup.find(hash);
                [[unlikely]] if (it == type_pool_lookup.end()) {
                    // The pool wasn't found so create it.
                    // create_component_pool takes a unique lock, so unlock the
                    // shared lock during its call
                    lock.unlock();
                    return create_component_pool<NakedType>();
                }
                else {
                    return it->second;
                }
            });

            return *static_cast<component_pool<NakedType>*>(pool);
        }

        // Const lambda
        template<int Group, typename ExePolicy, typename UpdateFn, typename R, typename C, typename... Args>
        auto& create_system(UpdateFn update_func, R (C::*)(Args...) const) {
            return create_system<Group, ExePolicy, UpdateFn, nullptr_t, Args...>(update_func, nullptr);
        }

        // Const lambda with sort
        template<int Group, typename ExePolicy, typename UpdateFn, typename SortFn, typename R, typename C,
            typename... Args>
        auto& create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(Args...) const) {
            return create_system<Group, ExePolicy, UpdateFn, SortFn, Args...>(update_func, sort_func);
        }

        // Mutable lambda
        template<int Group, typename ExePolicy, typename UpdateFn, typename R, typename C, typename... Args>
        auto& create_system(UpdateFn update_func, R (C::*)(Args...)) {
            return create_system<Group, ExePolicy, UpdateFn, nullptr_t, Args...>(update_func, nullptr);
        }

        // Mutable lambda with sort
        template<int Group, typename ExePolicy, typename UpdateFn, typename SortFn, typename R, typename C,
            typename... Args>
        auto& create_system(UpdateFn update_func, SortFn sort_func, R (C::*)(Args...)) {
            return create_system<Group, ExePolicy, UpdateFn, SortFn, Args...>(update_func, sort_func);
        }

    private:
        template<int Group, typename ExePolicy, typename UpdateFn, typename SortFn, typename FirstArg, typename... Args>
        auto& create_system(UpdateFn update_func, SortFn sort_func) {
            // Set up the implementation
            using typed_system = system<Group, ExePolicy, UpdateFn, SortFn, FirstArg, Args...>;

            // Is the first argument an entity of sorts?
            bool constexpr has_entity = std::is_same_v<FirstArg, entity_id> || std::is_same_v<FirstArg, entity>;

            // Create the system instance
            std::unique_ptr<system_base> sys;
            if constexpr (has_entity) {
                sys = std::make_unique<typed_system>(update_func, sort_func, &get_component_pool<Args>()...);
            } else {
                sys = std::make_unique<typed_system>(
                    update_func, sort_func, &get_component_pool<FirstArg>(), &get_component_pool<Args>()...);
            }

            std::unique_lock lock(mutex);
            systems.push_back(std::move(sys));
            system_base* ptr_system = systems.back().get();
            Ensures(ptr_system != nullptr);

            sched.insert(ptr_system);

            return *ptr_system;
        }

        // Create a component pool for a new type
        template<typename T>
        component_pool_base* create_component_pool() {
            // Create a new pool if one does not already exist
            if (!has_component_pool<T>()) {
                std::unique_lock lock(mutex);

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

#endif // !__CONTEXT
#ifndef __RUNTIME
#define __RUNTIME

#include <concepts>
#include <execution>
#include <type_traits>
#include <utility>


namespace ecs {
    // Add components generated from an initializer function to a range of entities. Will not be
    // added until 'commit_changes()' is called. The initializer function signature must be
    //   T(ecs::entity_id)
    // where T is the component type returned by the function.
    // Pre: entity does not already have the component, or have it in queue to be added
    // template<typename Callable> requires std::invocable<Callable, entity_id>
    template<std::invocable<entity_id> Callable>
    void add_component(entity_range const range, Callable&& func) {
        // Return type of 'func'
        using ComponentType = decltype(std::declval<Callable>()(entity_id{0}));
        static_assert(!std::is_same_v<ComponentType, void>, "Initializer functions must return a component");

        // Add it to the component pool
        detail::component_pool<ComponentType>& pool = detail::_context.get_component_pool<ComponentType>();
        pool.add_init(range, std::forward<Callable>(func));
    }

    // Add a component to a range of entities. Will not be added until 'commit_changes()' is called.
    // Pre: entity does not already have the component, or have it in queue to be added
    template<typename T>
    void add_component(entity_range const range, T&& val) {
        static_assert(!std::is_reference_v<T>, "can not store references; pass a copy instead");
        static_assert(std::copyable<T>, "T must be copyable");

        // Add it to the component pool
        detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
        pool.add(range, std::forward<T>(val));
    }

    // Add a component to an entity. Will not be added until 'commit_changes()' is called.
    // Pre: entity does not already have the component, or have it in queue to be added
    template<typename T>
    void add_component(entity_id const id, T&& val) {
        add_component({id, id}, std::forward<T>(val));
    }

    // Add several components to a range of entities. Will not be added until 'commit_changes()' is
    // called. Pre: entity does not already have the component, or have it in queue to be added
    template<typename... T>
    void add_components(entity_range const range, T&&... vals) {
        static_assert(detail::unique<T...>, "the same component was specified more than once");
        (add_component(range, std::forward<T>(vals)), ...);
    }

    // Add several components to an entity. Will not be added until 'commit_changes()' is called.
    // Pre: entity does not already have the component, or have it in queue to be added
    template<typename... T>
    void add_components(entity_id const id, T&&... vals) {
        static_assert(detail::unique<T...>, "the same component was specified more than once");
        (add_component(id, std::forward<T>(vals)), ...);
    }

    // Removes a component from a range of entities. Will not be removed until 'commit_changes()' is
    // called. Pre: entity has the component
    template<detail::persistent T>
    void remove_component(entity_range const range) {
        // Remove the entities from the components pool
        detail::component_pool<T>& pool = detail::_context.get_component_pool<T>();
        pool.remove_range(range);
    }

    // Removes a component from an entity. Will not be removed until 'commit_changes()' is called.
    // Pre: entity has the component
    template<typename T>
    void remove_component(entity_id const id) {
        remove_component<T>({id, id});
    }

    // Returns a shared component. Can be called before a system for it has been added
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

    // Returns the number of active components
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
    inline void commit_changes() { detail::_context.commit_changes(); }

    // Calls the 'update' function on all the systems in the order they were added.
    inline void run_systems() { detail::_context.run_systems(); }

    // Commits all changes and calls the 'update' function on all the systems in the order they were
    // added. Same as calling commit_changes() and run_systems().
    inline void update_systems() {
        commit_changes();
        run_systems();
    }

    // Make a new system
    template<int Group = 0, detail::lambda UpdateFn>
    auto& make_system(UpdateFn update_func) {
        return detail::_context.create_system<Group, std::execution::sequenced_policy, UpdateFn>(
            update_func, &UpdateFn::operator());
    }
    // template<int Group = 0>
    // auto& make_system(detail::lambda auto update_func) {
    //     return detail::_context.create_system<Group, std::execution::sequenced_policy, UpdateFn>(
    //         update_func, &UpdateFn::operator());
    // }

    // Make a new system with a sort function attached
    template<int Group = 0, detail::lambda UpdateFn, detail::sorter SortFn>
    auto& make_system(UpdateFn update_func, SortFn sort_func) {
        return detail::_context.create_system<Group, std::execution::sequenced_policy, UpdateFn, SortFn>(
            update_func, sort_func, &UpdateFn::operator());
    }

    // Make a new system. It will process components in parallel.
    template<int Group = 0, detail::lambda UpdateFn>
    auto& make_parallel_system(UpdateFn update_func) {
        return detail::_context.create_system<Group, std::execution::parallel_unsequenced_policy, UpdateFn>(
            update_func, &UpdateFn::operator());
    }

    // Make a new system. It will process components in parallel.
    template<int Group = 0, detail::lambda UpdateFn, detail::sorter SortFn>
    auto& make_parallel_system(UpdateFn update_func, SortFn sort_func) {
        return detail::_context.create_system<Group, std::execution::parallel_unsequenced_policy, UpdateFn, SortFn>(
            update_func, sort_func, &UpdateFn::operator());
    }
} // namespace ecs

#endif // !__RUNTIME

#pragma once

#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <array>
#include <type_traits>
#include <concepts>
#include <bit>
#include <cstdint>
#include <unordered_map>
#include <typeindex>
#include <shared_mutex>
#include <functional>
#include <algorithm>
#include "binding.hpp"
#include "common.hpp"

namespace atom::extra {

/**
 * @class Container
 * @brief A dependency injection container for managing bindings and resolving
 * dependencies.
 * @tparam SymbolTypes The symbol types associated with the container.
 */
template <typename... SymbolTypes>
class Container {
public:
    using BindingMap =
        std::tuple<Binding<SymbolTypes, SymbolTypes...>...>;  ///< The map of
                                                              ///< bindings.

    /**
     * @brief Binds a symbol to a value or factory.
     * @tparam T The symbol type to bind.
     * @return A reference to the BindingTo object for further configuration.
     */
    template <Symbolic T>
    BindingTo<typename T::value, SymbolTypes...>& bind() {
        static_assert((std::is_same_v<T, SymbolTypes> || ...),
                      "atom::extra::Container symbol not registered");
        return std::get<Binding<T, SymbolTypes...>>(bindings_);
    }

    /**
     * @brief Resolves a value for a given symbol.
     * @tparam T The symbol type to resolve.
     * @return The resolved value.
     */
    template <Symbolic T>
    typename T::value get() {
        return get<T>(Tag(""));
    }

    /**
     * @brief Resolves a value for a given symbol and tag.
     * @tparam T The symbol type to resolve.
     * @param tag The tag to match.
     * @return The resolved value.
     * @throws exceptions::ResolutionException if no matching binding is found.
     */
    template <Symbolic T>
    typename T::value get(const Tag& tag) {
        static_assert((std::is_same_v<T, SymbolTypes> || ...),
                      "atom::extra::Container symbol not registered");
        auto& binding = std::get<Binding<T, SymbolTypes...>>(bindings_);
        if (binding.matchesTag(tag)) {
            return binding.resolve(context_);
        }
        throw exceptions::ResolutionException(
            "No matching binding found for the given tag.");
    }

    /**
     * @brief Resolves a value for a given symbol and name.
     * @tparam T The symbol type to resolve.
     * @param name The name to match.
     * @return The resolved value.
     * @throws exceptions::ResolutionException if no matching binding is found.
     */
    template <Symbolic T>
    typename T::value getNamed(const std::string& name) {
        static_assert((std::is_same_v<T, SymbolTypes> || ...),
                      "atom::extra::Container symbol not registered");
        auto& binding = std::get<Binding<T, SymbolTypes...>>(bindings_);
        if (binding.matchesTargetName(name)) {
            return binding.resolve(context_);
        }
        throw exceptions::ResolutionException(
            "No matching binding found for the given name.");
    }

    /**
     * @brief Resolves all values for a given symbol.
     * @tparam T The symbol type to resolve.
     * @return A vector of resolved values.
     */
    template <Symbolic T>
    std::vector<typename T::value> getAll() {
        static_assert((std::is_same_v<T, SymbolTypes> || ...),
                      "atom::extra::Container symbol not registered");
        std::vector<typename T::value> result;
        auto& binding = std::get<Binding<T, SymbolTypes...>>(bindings_);
        result.push_back(binding.resolve(context_));
        return result;
    }

    /**
     * @brief Checks if a binding exists for a given symbol.
     * @tparam T The symbol type to check.
     * @return True if a binding exists, false otherwise.
     */
    template <Symbolic T>
    bool hasBinding() const {
        return std::get<Binding<T, SymbolTypes...>>(bindings_).resolver_ !=
               nullptr;
    }

    /**
     * @brief Unbinds a symbol, removing its binding.
     * @tparam T The symbol type to unbind.
     */
    template <Symbolic T>
    void unbind() {
        std::get<Binding<T, SymbolTypes...>>(bindings_).resolver_.reset();
    }

    /**
     * @brief Creates a child container that inherits bindings from the parent.
     * @return A unique pointer to the child container.
     */
    std::unique_ptr<Container> createChildContainer() {
        auto child = std::make_unique<Container>();
        child->parent_ = this;
        return child;
    }

private:
    BindingMap bindings_;  ///< The map of bindings.
    Context<SymbolTypes...> context_{
        *this};                    ///< The context for resolving dependencies.
    Container* parent_ = nullptr;  ///< The parent container, if any.
};

// ============================================================================
// LOCK-FREE DATA STRUCTURES
// ============================================================================

namespace lockfree {

/**
 * @brief Memory ordering utilities for lock-free programming
 */
namespace memory_order {
    constexpr auto relaxed = std::memory_order_relaxed;
    constexpr auto consume = std::memory_order_consume;
    constexpr auto acquire = std::memory_order_acquire;
    constexpr auto release = std::memory_order_release;
    constexpr auto acq_rel = std::memory_order_acq_rel;
    constexpr auto seq_cst = std::memory_order_seq_cst;
}

/**
 * @brief Hardware-specific optimizations for different architectures
 */
namespace hardware {
    inline void cpu_pause() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__)
        __asm__ __volatile__("yield" ::: "memory");
#else
        std::this_thread::yield();
#endif
    }

    inline void memory_fence() noexcept {
        std::atomic_thread_fence(memory_order::seq_cst);
    }

    inline void compiler_barrier() noexcept {
        std::atomic_signal_fence(memory_order::seq_cst);
    }
}

/**
 * @brief High-performance lock-free queue using Michael & Scott algorithm
 * @tparam T Element type
 * @tparam Allocator Custom allocator for nodes
 */
template<typename T, typename Allocator = std::allocator<T>>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<T*> data{nullptr};
        std::atomic<Node*> next{nullptr};

        Node() = default;
        explicit Node(T&& item) : data(new T(std::move(item))) {}
        explicit Node(const T& item) : data(new T(item)) {}
    };

    alignas(64) std::atomic<Node*> head_;
    alignas(64) std::atomic<Node*> tail_;

    using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    NodeAllocator node_allocator_;

public:
    /**
     * @brief Constructs an empty lock-free queue
     */
    explicit LockFreeQueue(const Allocator& alloc = Allocator{})
        : node_allocator_(alloc) {
        Node* dummy = std::allocator_traits<NodeAllocator>::allocate(node_allocator_, 1);
        std::allocator_traits<NodeAllocator>::construct(node_allocator_, dummy);

        head_.store(dummy, memory_order::relaxed);
        tail_.store(dummy, memory_order::relaxed);

#if ATOM_HAS_SPDLOG
        spdlog::debug("LockFreeQueue initialized with dummy node at {}",
                     static_cast<void*>(dummy));
#endif
    }

    /**
     * @brief Destructor - cleans up remaining nodes
     */
    ~LockFreeQueue() {
        while (Node* const old_head = head_.load(memory_order::relaxed)) {
            head_.store(old_head->next.load(memory_order::relaxed), memory_order::relaxed);
            if (old_head->data.load(memory_order::relaxed)) {
                delete old_head->data.load(memory_order::relaxed);
            }
            std::allocator_traits<NodeAllocator>::destroy(node_allocator_, old_head);
            std::allocator_traits<NodeAllocator>::deallocate(node_allocator_, old_head, 1);
        }
    }

    /**
     * @brief Enqueues an element (thread-safe)
     * @param item Element to enqueue
     */
    void enqueue(T item) {
        Node* new_node = std::allocator_traits<NodeAllocator>::allocate(node_allocator_, 1);
        std::allocator_traits<NodeAllocator>::construct(node_allocator_, new_node, std::move(item));

        while (true) {
            Node* last = tail_.load(memory_order::acquire);
            Node* next = last->next.load(memory_order::acquire);

            if (last == tail_.load(memory_order::acquire)) {
                if (next == nullptr) {
                    if (last->next.compare_exchange_weak(next, new_node,
                                                       memory_order::release,
                                                       memory_order::relaxed)) {
                        break;
                    }
                } else {
                    tail_.compare_exchange_weak(last, next,
                                              memory_order::release,
                                              memory_order::relaxed);
                }
            }
            hardware::cpu_pause();
        }

        tail_.compare_exchange_weak(tail_.load(memory_order::acquire), new_node,
                                   memory_order::release, memory_order::relaxed);
    }

    /**
     * @brief Attempts to dequeue an element (thread-safe)
     * @param result Reference to store the dequeued element
     * @return true if successful, false if queue is empty
     */
    bool try_dequeue(T& result) {
        while (true) {
            Node* first = head_.load(memory_order::acquire);
            Node* last = tail_.load(memory_order::acquire);
            Node* next = first->next.load(memory_order::acquire);

            if (first == head_.load(memory_order::acquire)) {
                if (first == last) {
                    if (next == nullptr) {
                        return false; // Queue is empty
                    }
                    tail_.compare_exchange_weak(last, next,
                                              memory_order::release,
                                              memory_order::relaxed);
                } else {
                    if (next == nullptr) {
                        continue;
                    }

                    T* data = next->data.load(memory_order::acquire);
                    if (data == nullptr) {
                        continue;
                    }

                    if (head_.compare_exchange_weak(first, next,
                                                  memory_order::release,
                                                  memory_order::relaxed)) {
                        result = *data;
                        delete data;
                        std::allocator_traits<NodeAllocator>::destroy(node_allocator_, first);
                        std::allocator_traits<NodeAllocator>::deallocate(node_allocator_, first, 1);
                        return true;
                    }
                }
            }
            hardware::cpu_pause();
        }
    }

    /**
     * @brief Checks if the queue is empty (approximate)
     * @return true if queue appears empty
     */
    bool empty() const noexcept {
        Node* first = head_.load(memory_order::acquire);
        Node* last = tail_.load(memory_order::acquire);
        return (first == last) && (first->next.load(memory_order::acquire) == nullptr);
    }

    // Non-copyable and non-movable
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
    LockFreeQueue(LockFreeQueue&&) = delete;
    LockFreeQueue& operator=(LockFreeQueue&&) = delete;
};

/**
 * @brief High-performance lock-free stack using Treiber's algorithm
 * @tparam T Element type
 * @tparam Allocator Custom allocator for nodes
 */
template<typename T, typename Allocator = std::allocator<T>>
class LockFreeStack {
private:
    struct Node {
        T data;
        std::atomic<Node*> next;

        template<typename... Args>
        explicit Node(Args&&... args) : data(std::forward<Args>(args)...), next(nullptr) {}
    };

    alignas(64) std::atomic<Node*> head_{nullptr};

    using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    NodeAllocator node_allocator_;

public:
    /**
     * @brief Constructs an empty lock-free stack
     */
    explicit LockFreeStack(const Allocator& alloc = Allocator{})
        : node_allocator_(alloc) {
#if ATOM_HAS_SPDLOG
        spdlog::debug("LockFreeStack initialized");
#endif
    }

    /**
     * @brief Destructor - cleans up remaining nodes
     */
    ~LockFreeStack() {
        while (Node* old_head = head_.load(memory_order::relaxed)) {
            head_.store(old_head->next.load(memory_order::relaxed), memory_order::relaxed);
            std::allocator_traits<NodeAllocator>::destroy(node_allocator_, old_head);
            std::allocator_traits<NodeAllocator>::deallocate(node_allocator_, old_head, 1);
        }
    }

    /**
     * @brief Pushes an element onto the stack (thread-safe)
     * @param item Element to push
     */
    void push(T item) {
        Node* new_node = std::allocator_traits<NodeAllocator>::allocate(node_allocator_, 1);
        std::allocator_traits<NodeAllocator>::construct(node_allocator_, new_node, std::move(item));

        Node* old_head = head_.load(memory_order::relaxed);
        do {
            new_node->next.store(old_head, memory_order::relaxed);
        } while (!head_.compare_exchange_weak(old_head, new_node,
                                            memory_order::release,
                                            memory_order::relaxed));
    }

    /**
     * @brief Attempts to pop an element from the stack (thread-safe)
     * @param result Reference to store the popped element
     * @return true if successful, false if stack is empty
     */
    bool try_pop(T& result) {
        Node* old_head = head_.load(memory_order::acquire);
        while (old_head && !head_.compare_exchange_weak(old_head,
                                                       old_head->next.load(memory_order::relaxed),
                                                       memory_order::release,
                                                       memory_order::relaxed)) {
            hardware::cpu_pause();
        }

        if (!old_head) {
            return false;
        }

        result = std::move(old_head->data);
        std::allocator_traits<NodeAllocator>::destroy(node_allocator_, old_head);
        std::allocator_traits<NodeAllocator>::deallocate(node_allocator_, old_head, 1);
        return true;
    }

    /**
     * @brief Checks if the stack is empty (approximate)
     * @return true if stack appears empty
     */
    bool empty() const noexcept {
        return head_.load(memory_order::acquire) == nullptr;
    }

    // Non-copyable and non-movable
    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;
    LockFreeStack(LockFreeStack&&) = delete;
    LockFreeStack& operator=(LockFreeStack&&) = delete;
};

/**
 * @brief High-performance lock-free ring buffer for single producer, single consumer
 * @tparam T Element type
 * @tparam Size Buffer size (must be power of 2)
 */
template<typename T, size_t Size>
requires (Size > 0 && (Size & (Size - 1)) == 0) // Power of 2 check
class LockFreeRingBuffer {
private:
    static constexpr size_t MASK = Size - 1;

    alignas(64) std::atomic<size_t> write_pos_{0};
    alignas(64) std::atomic<size_t> read_pos_{0};
    alignas(64) std::array<T, Size> buffer_;

public:
    /**
     * @brief Constructs an empty ring buffer
     */
    LockFreeRingBuffer() = default;

    /**
     * @brief Attempts to push an element (single producer)
     * @param item Element to push
     * @return true if successful, false if buffer is full
     */
    bool try_push(const T& item) noexcept {
        const size_t current_write = write_pos_.load(memory_order::relaxed);
        const size_t next_write = (current_write + 1) & MASK;

        if (next_write == read_pos_.load(memory_order::acquire)) {
            return false; // Buffer is full
        }

        buffer_[current_write] = item;
        write_pos_.store(next_write, memory_order::release);
        return true;
    }

    /**
     * @brief Attempts to push an element (single producer, move semantics)
     * @param item Element to push
     * @return true if successful, false if buffer is full
     */
    bool try_push(T&& item) noexcept {
        const size_t current_write = write_pos_.load(memory_order::relaxed);
        const size_t next_write = (current_write + 1) & MASK;

        if (next_write == read_pos_.load(memory_order::acquire)) {
            return false; // Buffer is full
        }

        buffer_[current_write] = std::move(item);
        write_pos_.store(next_write, memory_order::release);
        return true;
    }

    /**
     * @brief Attempts to pop an element (single consumer)
     * @param result Reference to store the popped element
     * @return true if successful, false if buffer is empty
     */
    bool try_pop(T& result) noexcept {
        const size_t current_read = read_pos_.load(memory_order::relaxed);

        if (current_read == write_pos_.load(memory_order::acquire)) {
            return false; // Buffer is empty
        }

        result = std::move(buffer_[current_read]);
        read_pos_.store((current_read + 1) & MASK, memory_order::release);
        return true;
    }

    /**
     * @brief Checks if the buffer is empty
     * @return true if buffer is empty
     */
    bool empty() const noexcept {
        return read_pos_.load(memory_order::acquire) == write_pos_.load(memory_order::acquire);
    }

    /**
     * @brief Checks if the buffer is full
     * @return true if buffer is full
     */
    bool full() const noexcept {
        const size_t next_write = (write_pos_.load(memory_order::acquire) + 1) & MASK;
        return next_write == read_pos_.load(memory_order::acquire);
    }

    /**
     * @brief Gets the current size of the buffer
     * @return Number of elements in buffer
     */
    size_t size() const noexcept {
        const size_t write = write_pos_.load(memory_order::acquire);
        const size_t read = read_pos_.load(memory_order::acquire);
        return (write - read) & MASK;
    }

    /**
     * @brief Gets the capacity of the buffer
     * @return Maximum number of elements
     */
    static constexpr size_t capacity() noexcept {
        return Size - 1; // One slot reserved for full/empty distinction
    }

    // Non-copyable and non-movable
    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer(LockFreeRingBuffer&&) = delete;
    LockFreeRingBuffer& operator=(LockFreeRingBuffer&&) = delete;
};

/**
 * @brief Lock-free hash map using open addressing and linear probing
 * @tparam Key Key type
 * @tparam Value Value type
 * @tparam Hash Hash function
 * @tparam KeyEqual Key equality predicate
 * @tparam Size Hash table size (must be power of 2)
 */
template<typename Key, typename Value, typename Hash = std::hash<Key>,
         typename KeyEqual = std::equal_to<Key>, size_t Size = 1024>
requires (Size > 0 && (Size & (Size - 1)) == 0) // Power of 2 check
class LockFreeHashMap {
private:
    struct Entry {
        std::atomic<Key> key;
        std::atomic<Value> value;
        std::atomic<bool> occupied{false};

        Entry() = default;
    };

    static constexpr size_t MASK = Size - 1;
    static constexpr Key EMPTY_KEY = Key{};

    alignas(64) std::array<Entry, Size> table_;
    Hash hasher_;
    KeyEqual key_equal_;

    size_t hash_key(const Key& key) const noexcept {
        return hasher_(key) & MASK;
    }

public:
    /**
     * @brief Constructs an empty hash map
     */
    LockFreeHashMap() = default;

    /**
     * @brief Inserts or updates a key-value pair
     * @param key Key to insert/update
     * @param value Value to associate with key
     * @return true if inserted, false if updated existing key
     */
    bool insert_or_update(const Key& key, const Value& value) {
        size_t index = hash_key(key);

        for (size_t i = 0; i < Size; ++i) {
            Entry& entry = table_[(index + i) & MASK];

            // Try to claim an empty slot
            bool expected = false;
            if (entry.occupied.compare_exchange_weak(expected, true,
                                                   memory_order::acq_rel,
                                                   memory_order::relaxed)) {
                entry.key.store(key, memory_order::release);
                entry.value.store(value, memory_order::release);
                return true; // Inserted new entry
            }

            // Check if this is the same key
            if (key_equal_(entry.key.load(memory_order::acquire), key)) {
                entry.value.store(value, memory_order::release);
                return false; // Updated existing entry
            }
        }

#if ATOM_HAS_SPDLOG
        spdlog::warn("LockFreeHashMap is full, cannot insert key");
#endif
        return false; // Table is full
    }

    /**
     * @brief Attempts to find a value by key
     * @param key Key to search for
     * @param result Reference to store the found value
     * @return true if found, false otherwise
     */
    bool find(const Key& key, Value& result) const {
        size_t index = hash_key(key);

        for (size_t i = 0; i < Size; ++i) {
            const Entry& entry = table_[(index + i) & MASK];

            if (!entry.occupied.load(memory_order::acquire)) {
                return false; // Empty slot, key not found
            }

            if (key_equal_(entry.key.load(memory_order::acquire), key)) {
                result = entry.value.load(memory_order::acquire);
                return true;
            }
        }

        return false; // Key not found
    }

    /**
     * @brief Attempts to remove a key-value pair
     * @param key Key to remove
     * @return true if removed, false if not found
     */
    bool erase(const Key& key) {
        size_t index = hash_key(key);

        for (size_t i = 0; i < Size; ++i) {
            Entry& entry = table_[(index + i) & MASK];

            if (!entry.occupied.load(memory_order::acquire)) {
                return false; // Empty slot, key not found
            }

            if (key_equal_(entry.key.load(memory_order::acquire), key)) {
                entry.occupied.store(false, memory_order::release);
                return true;
            }
        }

        return false; // Key not found
    }

    // Non-copyable and non-movable
    LockFreeHashMap(const LockFreeHashMap&) = delete;
    LockFreeHashMap& operator=(const LockFreeHashMap&) = delete;
    LockFreeHashMap(LockFreeHashMap&&) = delete;
    LockFreeHashMap& operator=(LockFreeHashMap&&) = delete;
};

} // namespace lockfree

// ============================================================================
// SYNCHRONIZATION PRIMITIVES
// ============================================================================

namespace sync {

/**
 * @brief Adaptive spinlock with exponential backoff and yield strategies
 */
class AdaptiveSpinLock {
private:
    alignas(64) std::atomic<bool> locked_{false};
    alignas(64) std::atomic<uint32_t> spin_count_{0};

    static constexpr uint32_t MAX_SPIN_COUNT = 4000;
    static constexpr uint32_t YIELD_THRESHOLD = 100;

    void cpu_pause() const noexcept {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__)
        __asm__ __volatile__("yield" ::: "memory");
#else
        std::this_thread::yield();
#endif
    }

public:
    /**
     * @brief Acquires the lock with adaptive spinning strategy
     */
    void lock() noexcept {
        uint32_t spin_count = 0;
        uint32_t backoff = 1;

        while (locked_.exchange(true, std::memory_order_acquire)) {
            ++spin_count;

            if (spin_count < YIELD_THRESHOLD) {
                // Active spinning with exponential backoff
                for (uint32_t i = 0; i < backoff; ++i) {
                    cpu_pause();
                }
                backoff = std::min(backoff * 2, 64u);
            } else if (spin_count < MAX_SPIN_COUNT) {
                // Yield to other threads
                std::this_thread::yield();
            } else {
                // Sleep for a short duration
                std::this_thread::sleep_for(std::chrono::microseconds(1));
                backoff = 1; // Reset backoff
            }
        }

        // Update global spin statistics
        spin_count_.fetch_add(spin_count, std::memory_order_relaxed);
    }

    /**
     * @brief Attempts to acquire the lock without blocking
     * @return true if lock was acquired, false otherwise
     */
    bool try_lock() noexcept {
        return !locked_.exchange(true, std::memory_order_acquire);
    }

    /**
     * @brief Releases the lock
     */
    void unlock() noexcept {
        locked_.store(false, std::memory_order_release);
    }

    /**
     * @brief Gets the total spin count for performance analysis
     * @return Total number of spins across all lock acquisitions
     */
    uint32_t get_spin_count() const noexcept {
        return spin_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Resets the spin count statistics
     */
    void reset_stats() noexcept {
        spin_count_.store(0, std::memory_order_relaxed);
    }
};

/**
 * @brief High-performance reader-writer lock with priority inheritance
 */
class ReaderWriterLock {
private:
    alignas(64) std::atomic<int32_t> reader_count_{0};
    alignas(64) std::atomic<bool> writer_waiting_{false};
    alignas(64) std::atomic<bool> writer_active_{false};

    static constexpr int32_t WRITER_FLAG = 0x40000000;
    static constexpr int32_t READER_MASK = 0x3FFFFFFF;

public:
    /**
     * @brief Acquires a shared (read) lock
     */
    void lock_shared() noexcept {
        while (true) {
            // Wait for any active writer to finish
            while (writer_active_.load(std::memory_order_acquire) ||
                   writer_waiting_.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            // Try to increment reader count
            int32_t expected = reader_count_.load(std::memory_order_relaxed);
            if (expected >= 0 &&
                reader_count_.compare_exchange_weak(expected, expected + 1,
                                                  std::memory_order_acquire,
                                                  std::memory_order_relaxed)) {
                // Double-check no writer became active
                if (!writer_active_.load(std::memory_order_acquire)) {
                    return; // Successfully acquired read lock
                }

                // Writer became active, release our read lock
                reader_count_.fetch_sub(1, std::memory_order_release);
            }

            std::this_thread::yield();
        }
    }

    /**
     * @brief Attempts to acquire a shared (read) lock without blocking
     * @return true if lock was acquired, false otherwise
     */
    bool try_lock_shared() noexcept {
        if (writer_active_.load(std::memory_order_acquire) ||
            writer_waiting_.load(std::memory_order_acquire)) {
            return false;
        }

        int32_t expected = reader_count_.load(std::memory_order_relaxed);
        return expected >= 0 &&
               reader_count_.compare_exchange_strong(expected, expected + 1,
                                                   std::memory_order_acquire,
                                                   std::memory_order_relaxed) &&
               !writer_active_.load(std::memory_order_acquire);
    }

    /**
     * @brief Releases a shared (read) lock
     */
    void unlock_shared() noexcept {
        reader_count_.fetch_sub(1, std::memory_order_release);
    }

    /**
     * @brief Acquires an exclusive (write) lock
     */
    void lock() noexcept {
        // Signal that a writer is waiting
        writer_waiting_.store(true, std::memory_order_release);

        // Wait for all readers to finish
        while (reader_count_.load(std::memory_order_acquire) > 0) {
            std::this_thread::yield();
        }

        // Acquire exclusive access
        bool expected = false;
        while (!writer_active_.compare_exchange_weak(expected, true,
                                                   std::memory_order_acquire,
                                                   std::memory_order_relaxed)) {
            expected = false;
            std::this_thread::yield();
        }

        writer_waiting_.store(false, std::memory_order_release);
    }

    /**
     * @brief Attempts to acquire an exclusive (write) lock without blocking
     * @return true if lock was acquired, false otherwise
     */
    bool try_lock() noexcept {
        if (reader_count_.load(std::memory_order_acquire) > 0) {
            return false;
        }

        bool expected = false;
        return writer_active_.compare_exchange_strong(expected, true,
                                                    std::memory_order_acquire,
                                                    std::memory_order_relaxed);
    }

    /**
     * @brief Releases an exclusive (write) lock
     */
    void unlock() noexcept {
        writer_active_.store(false, std::memory_order_release);
    }

    /**
     * @brief Gets the current number of active readers
     * @return Number of active readers
     */
    int32_t reader_count() const noexcept {
        return reader_count_.load(std::memory_order_acquire);
    }

    /**
     * @brief Checks if a writer is currently active
     * @return true if writer is active
     */
    bool writer_active() const noexcept {
        return writer_active_.load(std::memory_order_acquire);
    }
};

/**
 * @brief RAII wrapper for reader-writer lock shared access
 */
class SharedLockGuard {
private:
    ReaderWriterLock& lock_;

public:
    explicit SharedLockGuard(ReaderWriterLock& lock) : lock_(lock) {
        lock_.lock_shared();
    }

    ~SharedLockGuard() {
        lock_.unlock_shared();
    }

    // Non-copyable and non-movable
    SharedLockGuard(const SharedLockGuard&) = delete;
    SharedLockGuard& operator=(const SharedLockGuard&) = delete;
    SharedLockGuard(SharedLockGuard&&) = delete;
    SharedLockGuard& operator=(SharedLockGuard&&) = delete;
};

/**
 * @brief RAII wrapper for reader-writer lock exclusive access
 */
class ExclusiveLockGuard {
private:
    ReaderWriterLock& lock_;

public:
    explicit ExclusiveLockGuard(ReaderWriterLock& lock) : lock_(lock) {
        lock_.lock();
    }

    ~ExclusiveLockGuard() {
        lock_.unlock();
    }

    // Non-copyable and non-movable
    ExclusiveLockGuard(const ExclusiveLockGuard&) = delete;
    ExclusiveLockGuard& operator=(const ExclusiveLockGuard&) = delete;
    ExclusiveLockGuard(ExclusiveLockGuard&&) = delete;
    ExclusiveLockGuard& operator=(ExclusiveLockGuard&&) = delete;
};

/**
 * @brief Hazard pointer implementation for safe memory reclamation in lock-free data structures
 * @tparam T Type of objects being protected
 * @tparam MaxThreads Maximum number of threads that can use hazard pointers
 * @tparam MaxHazardPtrs Maximum number of hazard pointers per thread
 */
template<typename T, size_t MaxThreads = 64, size_t MaxHazardPtrs = 8>
class HazardPointers {
private:
    struct HazardRecord {
        alignas(64) std::atomic<T*> hazard_ptrs[MaxHazardPtrs];
        alignas(64) std::atomic<std::thread::id> thread_id{std::thread::id{}};
        alignas(64) std::atomic<bool> active{false};

        HazardRecord() {
            for (auto& ptr : hazard_ptrs) {
                ptr.store(nullptr, std::memory_order_relaxed);
            }
        }
    };

    struct RetiredNode {
        T* ptr;
        std::function<void(T*)> deleter;
        RetiredNode* next;

        RetiredNode(T* p, std::function<void(T*)> del)
            : ptr(p), deleter(std::move(del)), next(nullptr) {}
    };

    alignas(64) std::array<HazardRecord, MaxThreads> hazard_records_;
    alignas(64) std::atomic<RetiredNode*> retired_list_{nullptr};

    thread_local static HazardRecord* thread_record_;
    thread_local static std::array<RetiredNode*, 1000> retired_nodes_;
    thread_local static size_t retired_count_;

    HazardRecord* acquire_thread_record() {
        auto thread_id = std::this_thread::get_id();

        // Try to find existing record for this thread
        for (auto& record : hazard_records_) {
            auto expected_id = std::thread::id{};
            if (record.thread_id.compare_exchange_strong(expected_id, thread_id,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_relaxed)) {
                record.active.store(true, std::memory_order_release);
                return &record;
            }
            if (record.thread_id.load(std::memory_order_acquire) == thread_id) {
                return &record;
            }
        }

#if ATOM_HAS_SPDLOG
        spdlog::error("No available hazard pointer records for thread");
#endif
        return nullptr;
    }

    void scan_and_reclaim() {
        // Collect all hazard pointers
        std::array<T*, MaxThreads * MaxHazardPtrs> hazard_ptrs;
        size_t hazard_count = 0;

        for (const auto& record : hazard_records_) {
            if (record.active.load(std::memory_order_acquire)) {
                for (const auto& ptr : record.hazard_ptrs) {
                    T* hazard_ptr = ptr.load(std::memory_order_acquire);
                    if (hazard_ptr) {
                        hazard_ptrs[hazard_count++] = hazard_ptr;
                    }
                }
            }
        }

        // Sort hazard pointers for efficient searching
        std::sort(hazard_ptrs.begin(), hazard_ptrs.begin() + hazard_count);

        // Check retired nodes against hazard pointers
        for (size_t i = 0; i < retired_count_; ) {
            if (std::binary_search(hazard_ptrs.begin(),
                                 hazard_ptrs.begin() + hazard_count,
                                 retired_nodes_[i]->ptr)) {
                // Still hazardous, keep it
                ++i;
            } else {
                // Safe to delete
                auto* node = retired_nodes_[i];
                node->deleter(node->ptr);
                delete node;

                // Move last element to current position
                retired_nodes_[i] = retired_nodes_[--retired_count_];
            }
        }
    }

public:
    /**
     * @brief Constructs hazard pointer manager
     */
    HazardPointers() = default;

    /**
     * @brief Destructor - cleans up remaining retired nodes
     */
    ~HazardPointers() {
        // Clean up any remaining retired nodes
        RetiredNode* current = retired_list_.load(std::memory_order_acquire);
        while (current) {
            RetiredNode* next = current->next;
            current->deleter(current->ptr);
            delete current;
            current = next;
        }
    }

    /**
     * @brief Protects a pointer with a hazard pointer
     * @param slot Hazard pointer slot index (0 to MaxHazardPtrs-1)
     * @param ptr Pointer to protect
     */
    void protect(size_t slot, T* ptr) {
        if (!thread_record_) {
            thread_record_ = acquire_thread_record();
        }

        if (thread_record_ && slot < MaxHazardPtrs) {
            thread_record_->hazard_ptrs[slot].store(ptr, std::memory_order_release);
        }
    }

    /**
     * @brief Clears a hazard pointer slot
     * @param slot Hazard pointer slot index
     */
    void clear(size_t slot) {
        if (thread_record_ && slot < MaxHazardPtrs) {
            thread_record_->hazard_ptrs[slot].store(nullptr, std::memory_order_release);
        }
    }

    /**
     * @brief Retires a pointer for later deletion
     * @param ptr Pointer to retire
     * @param deleter Custom deleter function
     */
    void retire(T* ptr, std::function<void(T*)> deleter = [](T* p) { delete p; }) {
        retired_nodes_[retired_count_++] = new RetiredNode(ptr, std::move(deleter));

        if (retired_count_ >= 100) { // Threshold for cleanup
            scan_and_reclaim();
        }
    }

    /**
     * @brief Forces immediate scan and reclamation
     */
    void force_reclaim() {
        scan_and_reclaim();
    }

    // Non-copyable and non-movable
    HazardPointers(const HazardPointers&) = delete;
    HazardPointers& operator=(const HazardPointers&) = delete;
    HazardPointers(HazardPointers&&) = delete;
    HazardPointers& operator=(HazardPointers&&) = delete;
};

// Thread-local storage definitions
template<typename T, size_t MaxThreads, size_t MaxHazardPtrs>
thread_local typename HazardPointers<T, MaxThreads, MaxHazardPtrs>::HazardRecord*
    HazardPointers<T, MaxThreads, MaxHazardPtrs>::thread_record_ = nullptr;

template<typename T, size_t MaxThreads, size_t MaxHazardPtrs>
thread_local std::array<typename HazardPointers<T, MaxThreads, MaxHazardPtrs>::RetiredNode*, 1000>
    HazardPointers<T, MaxThreads, MaxHazardPtrs>::retired_nodes_;

template<typename T, size_t MaxThreads, size_t MaxHazardPtrs>
thread_local size_t HazardPointers<T, MaxThreads, MaxHazardPtrs>::retired_count_ = 0;

/**
 * @brief RAII wrapper for hazard pointer protection
 * @tparam T Type of object being protected
 */
template<typename T>
class HazardPointerGuard {
private:
    HazardPointers<T>& hp_manager_;
    size_t slot_;

public:
    /**
     * @brief Constructs guard and protects the pointer
     * @param hp_manager Hazard pointer manager
     * @param slot Slot index to use
     * @param ptr Pointer to protect
     */
    HazardPointerGuard(HazardPointers<T>& hp_manager, size_t slot, T* ptr)
        : hp_manager_(hp_manager), slot_(slot) {
        hp_manager_.protect(slot_, ptr);
    }

    /**
     * @brief Destructor - clears the hazard pointer
     */
    ~HazardPointerGuard() {
        hp_manager_.clear(slot_);
    }

    // Non-copyable and non-movable
    HazardPointerGuard(const HazardPointerGuard&) = delete;
    HazardPointerGuard& operator=(const HazardPointerGuard&) = delete;
    HazardPointerGuard(HazardPointerGuard&&) = delete;
    HazardPointerGuard& operator=(HazardPointerGuard&&) = delete;
};

} // namespace sync

// ============================================================================
// CONCURRENT CONTAINER IMPLEMENTATION
// ============================================================================

/**
 * @brief Thread-local cache entry for fast dependency resolution
 * @tparam T The cached value type
 */
template<typename T>
struct CacheEntry {
    alignas(64) std::atomic<T*> value{nullptr};
    alignas(64) std::atomic<uint64_t> version{0};
    alignas(64) std::atomic<std::chrono::steady_clock::time_point> timestamp;

    static constexpr std::chrono::milliseconds CACHE_TTL{100};

    CacheEntry() {
        timestamp.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
    }

    bool is_valid() const noexcept {
        auto now = std::chrono::steady_clock::now();
        auto cached_time = timestamp.load(std::memory_order_acquire);
        return (now - cached_time) < CACHE_TTL;
    }

    void invalidate() noexcept {
        value.store(nullptr, std::memory_order_release);
        version.fetch_add(1, std::memory_order_acq_rel);
    }
};

/**
 * @brief High-performance concurrent dependency injection container
 * @tparam SymbolTypes The symbol types supported by this container
 */
template<typename... SymbolTypes>
class ConcurrentContainer {
private:
    using BindingMap = std::tuple<Binding<SymbolTypes, SymbolTypes...>...>;
    using ReaderWriterLock = sync::ReaderWriterLock;
    using SharedLockGuard = sync::SharedLockGuard;
    using ExclusiveLockGuard = sync::ExclusiveLockGuard;

    // Core container state
    alignas(64) BindingMap bindings_;
    alignas(64) mutable ReaderWriterLock bindings_lock_;
    alignas(64) Context<SymbolTypes...> context_{*this};

    // Performance monitoring
    alignas(64) std::atomic<uint64_t> resolution_count_{0};
    alignas(64) std::atomic<uint64_t> cache_hits_{0};
    alignas(64) std::atomic<uint64_t> cache_misses_{0};
    alignas(64) std::atomic<uint64_t> global_version_{1};

    // Thread-local cache storage
    thread_local static std::unordered_map<std::type_index, std::unique_ptr<void, void(*)(void*)>> cache_;
    thread_local static std::atomic<uint64_t> cache_version_;

    /**
     * @brief Gets or creates a cache entry for the given type
     * @tparam T The type to cache
     * @return Reference to the cache entry
     */
    template<typename T>
    CacheEntry<T>& get_cache_entry() {
        auto type_index = std::type_index(typeid(T));
        auto it = cache_.find(type_index);

        if (it == cache_.end()) {
            auto deleter = [](void* ptr) {
                delete static_cast<CacheEntry<T>*>(ptr);
            };

            auto entry = std::make_unique<CacheEntry<T>>();
            auto* entry_ptr = entry.get();

            cache_[type_index] = std::unique_ptr<void, void(*)(void*)>(
                entry.release(), deleter);

            return *entry_ptr;
        }

        return *static_cast<CacheEntry<T>*>(it->second.get());
    }

    /**
     * @brief Invalidates all thread-local caches
     */
    void invalidate_caches() noexcept {
        global_version_.fetch_add(1, std::memory_order_acq_rel);

#if ATOM_HAS_SPDLOG
        spdlog::debug("Invalidated all dependency caches, new version: {}",
                     global_version_.load(std::memory_order_relaxed));
#endif
    }

public:
    /**
     * @brief Constructs a concurrent container
     */
    ConcurrentContainer() {
#if ATOM_HAS_SPDLOG
        spdlog::info("ConcurrentContainer initialized with {} symbol types",
                    sizeof...(SymbolTypes));
#endif
    }

    /**
     * @brief Destructor
     */
    ~ConcurrentContainer() {
#if ATOM_HAS_SPDLOG
        auto resolutions = resolution_count_.load(std::memory_order_relaxed);
        auto hits = cache_hits_.load(std::memory_order_relaxed);
        auto misses = cache_misses_.load(std::memory_order_relaxed);

        spdlog::info("ConcurrentContainer destroyed. Stats - Resolutions: {}, "
                    "Cache hits: {}, Cache misses: {}, Hit rate: {:.2f}%",
                    resolutions, hits, misses,
                    resolutions > 0 ? (100.0 * hits / resolutions) : 0.0);
#endif
    }

    /**
     * @brief Thread-safe binding configuration
     * @tparam T The symbol type to bind
     * @return Reference to the binding configuration object
     */
    template<Symbolic T>
    BindingTo<typename T::value, SymbolTypes...>& bind() {
        static_assert((std::is_same_v<T, SymbolTypes> || ...),
                      "Symbol type not registered with container");

        ExclusiveLockGuard lock(bindings_lock_);
        invalidate_caches();

        return std::get<Binding<T, SymbolTypes...>>(bindings_);
    }

    /**
     * @brief High-performance dependency resolution with caching
     * @tparam T The symbol type to resolve
     * @return The resolved dependency
     */
    template<Symbolic T>
    typename T::value get() {
        static_assert((std::is_same_v<T, SymbolTypes> || ...),
                      "Symbol type not registered with container");

        resolution_count_.fetch_add(1, std::memory_order_relaxed);

        // Check thread-local cache first
        auto& cache_entry = get_cache_entry<typename T::value>();
        auto current_version = global_version_.load(std::memory_order_acquire);

        if (cache_entry.is_valid() &&
            cache_entry.version.load(std::memory_order_acquire) == current_version) {

            auto* cached_value = cache_entry.value.load(std::memory_order_acquire);
            if (cached_value) {
                cache_hits_.fetch_add(1, std::memory_order_relaxed);
                return *cached_value;
            }
        }

        // Cache miss - resolve from binding
        cache_misses_.fetch_add(1, std::memory_order_relaxed);

        SharedLockGuard lock(bindings_lock_);
        auto& binding = std::get<Binding<T, SymbolTypes...>>(bindings_);

        if (!binding.resolver_) {
            throw exceptions::ResolutionException(
                "No binding found for requested type");
        }

        auto result = binding.resolver_->resolve(context_);

        // Update cache with resolved value
        if constexpr (std::is_copy_constructible_v<typename T::value>) {
            auto* cached_ptr = new typename T::value(result);
            cache_entry.value.store(cached_ptr, std::memory_order_release);
            cache_entry.version.store(current_version, std::memory_order_release);
            cache_entry.timestamp.store(std::chrono::steady_clock::now(),
                                       std::memory_order_release);
        }

        return result;
    }

    /**
     * @brief Checks if a binding exists for the given symbol
     * @tparam T The symbol type to check
     * @return true if binding exists
     */
    template<Symbolic T>
    bool has_binding() const {
        static_assert((std::is_same_v<T, SymbolTypes> || ...),
                      "Symbol type not registered with container");

        SharedLockGuard lock(bindings_lock_);
        const auto& binding = std::get<Binding<T, SymbolTypes...>>(bindings_);
        return binding.resolver_ != nullptr;
    }

    /**
     * @brief Removes a binding for the given symbol
     * @tparam T The symbol type to unbind
     */
    template<Symbolic T>
    void unbind() {
        static_assert((std::is_same_v<T, SymbolTypes> || ...),
                      "Symbol type not registered with container");

        ExclusiveLockGuard lock(bindings_lock_);
        auto& binding = std::get<Binding<T, SymbolTypes...>>(bindings_);
        binding.resolver_.reset();
        invalidate_caches();
    }

    /**
     * @brief Gets performance statistics
     * @return Tuple of (resolutions, cache_hits, cache_misses, hit_rate)
     */
    std::tuple<uint64_t, uint64_t, uint64_t, double> get_stats() const noexcept {
        auto resolutions = resolution_count_.load(std::memory_order_relaxed);
        auto hits = cache_hits_.load(std::memory_order_relaxed);
        auto misses = cache_misses_.load(std::memory_order_relaxed);
        double hit_rate = resolutions > 0 ? (100.0 * hits / resolutions) : 0.0;

        return std::make_tuple(resolutions, hits, misses, hit_rate);
    }

    /**
     * @brief Resets performance statistics
     */
    void reset_stats() noexcept {
        resolution_count_.store(0, std::memory_order_relaxed);
        cache_hits_.store(0, std::memory_order_relaxed);
        cache_misses_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Forces cache invalidation across all threads
     */
    void invalidate_all_caches() noexcept {
        invalidate_caches();
    }

    // Non-copyable and non-movable
    ConcurrentContainer(const ConcurrentContainer&) = delete;
    ConcurrentContainer& operator=(const ConcurrentContainer&) = delete;
    ConcurrentContainer(ConcurrentContainer&&) = delete;
    ConcurrentContainer& operator=(ConcurrentContainer&&) = delete;
};

// Thread-local storage definitions
template<typename... SymbolTypes>
thread_local std::unordered_map<std::type_index, std::unique_ptr<void, void(*)(void*)>>
    ConcurrentContainer<SymbolTypes...>::cache_;

template<typename... SymbolTypes>
thread_local std::atomic<uint64_t> ConcurrentContainer<SymbolTypes...>::cache_version_{0};

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

namespace memory {

/**
 * @brief Epoch-based memory management for safe cross-thread deallocation
 */
class EpochManager {
private:
    struct ThreadRecord {
        alignas(64) std::atomic<uint64_t> local_epoch{0};
        alignas(64) std::atomic<bool> active{false};
        alignas(64) std::atomic<std::thread::id> thread_id{std::thread::id{}};
    };

    static constexpr size_t MAX_THREADS = 128;
    static constexpr size_t EPOCH_FREQUENCY = 100;

    alignas(64) std::atomic<uint64_t> global_epoch_{1};
    alignas(64) std::array<ThreadRecord, MAX_THREADS> thread_records_;

    thread_local static ThreadRecord* thread_record_;
    thread_local static uint64_t operation_count_;

    ThreadRecord* acquire_thread_record() {
        auto thread_id = std::this_thread::get_id();

        for (auto& record : thread_records_) {
            auto expected_id = std::thread::id{};
            if (record.thread_id.compare_exchange_strong(expected_id, thread_id,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_relaxed)) {
                record.active.store(true, std::memory_order_release);
                return &record;
            }
            if (record.thread_id.load(std::memory_order_acquire) == thread_id) {
                return &record;
            }
        }

#if ATOM_HAS_SPDLOG
        spdlog::error("No available thread records for epoch management");
#endif
        return nullptr;
    }

public:
    /**
     * @brief Enters a critical section
     */
    void enter() {
        if (!thread_record_) {
            thread_record_ = acquire_thread_record();
        }

        if (thread_record_) {
            uint64_t global = global_epoch_.load(std::memory_order_acquire);
            thread_record_->local_epoch.store(global, std::memory_order_release);

            // Periodically advance global epoch
            if (++operation_count_ % EPOCH_FREQUENCY == 0) {
                global_epoch_.compare_exchange_weak(global, global + 1,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Exits a critical section
     */
    void exit() {
        if (thread_record_) {
            thread_record_->local_epoch.store(0, std::memory_order_release);
        }
    }

    /**
     * @brief Gets the minimum epoch across all active threads
     * @return Minimum epoch value
     */
    uint64_t get_min_epoch() const {
        uint64_t min_epoch = global_epoch_.load(std::memory_order_acquire);

        for (const auto& record : thread_records_) {
            if (record.active.load(std::memory_order_acquire)) {
                uint64_t local = record.local_epoch.load(std::memory_order_acquire);
                if (local > 0 && local < min_epoch) {
                    min_epoch = local;
                }
            }
        }

        return min_epoch;
    }

    /**
     * @brief Gets the current global epoch
     * @return Current global epoch
     */
    uint64_t get_global_epoch() const {
        return global_epoch_.load(std::memory_order_acquire);
    }
};

// Thread-local storage definitions
thread_local EpochManager::ThreadRecord* EpochManager::thread_record_ = nullptr;
thread_local uint64_t EpochManager::operation_count_ = 0;

/**
 * @brief RAII guard for epoch-based critical sections
 */
class EpochGuard {
private:
    EpochManager& manager_;

public:
    explicit EpochGuard(EpochManager& manager) : manager_(manager) {
        manager_.enter();
    }

    ~EpochGuard() {
        manager_.exit();
    }

    // Non-copyable and non-movable
    EpochGuard(const EpochGuard&) = delete;
    EpochGuard& operator=(const EpochGuard&) = delete;
    EpochGuard(EpochGuard&&) = delete;
    EpochGuard& operator=(EpochGuard&&) = delete;
};

/**
 * @brief High-performance thread-local memory pool with lock-free allocation
 * @tparam T Object type to allocate
 * @tparam ChunkSize Number of objects per chunk
 */
template<typename T, size_t ChunkSize = 1024>
class ThreadLocalPool {
private:
    struct FreeNode {
        FreeNode* next;
    };

    struct Chunk {
        alignas(alignof(T)) std::byte storage[sizeof(T) * ChunkSize];
        std::atomic<size_t> allocated_count{0};
        Chunk* next_chunk{nullptr};

        T* get_object(size_t index) {
            return reinterpret_cast<T*>(storage + index * sizeof(T));
        }
    };

    thread_local static Chunk* current_chunk_;
    thread_local static FreeNode* free_list_;
    thread_local static size_t next_allocation_index_;

    static EpochManager epoch_manager_;

    // Global list of chunks for cross-thread deallocation
    alignas(64) std::atomic<Chunk*> global_chunks_{nullptr};

public:
    /**
     * @brief Constructs a thread-local pool
     */
    ThreadLocalPool() = default;

private:
    Chunk* allocate_new_chunk() {
        auto* chunk = new Chunk();

        // Add to global chunk list
        Chunk* old_head = global_chunks_.load(std::memory_order_relaxed);
        do {
            chunk->next_chunk = old_head;
        } while (!global_chunks_.compare_exchange_weak(old_head, chunk,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed));

#if ATOM_HAS_SPDLOG
        spdlog::debug("Allocated new chunk for ThreadLocalPool<{}>", typeid(T).name());
#endif

        return chunk;
    }

public:
    /**
     * @brief Allocates an object from the thread-local pool
     * @return Pointer to allocated object
     */
    T* allocate() {
        EpochGuard guard(epoch_manager_);

        // Try to get from free list first
        if (free_list_) {
            FreeNode* node = free_list_;
            free_list_ = node->next;
            return reinterpret_cast<T*>(node);
        }

        // Allocate from current chunk
        if (!current_chunk_ || next_allocation_index_ >= ChunkSize) {
            current_chunk_ = allocate_new_chunk();
            next_allocation_index_ = 0;
        }

        T* result = current_chunk_->get_object(next_allocation_index_++);
        current_chunk_->allocated_count.fetch_add(1, std::memory_order_relaxed);

        return result;
    }

    /**
     * @brief Deallocates an object (can be called from any thread)
     * @param ptr Pointer to object to deallocate
     */
    void deallocate(T* ptr) {
        if (!ptr) return;

        EpochGuard guard(epoch_manager_);

        // Find which chunk this pointer belongs to
        Chunk* chunk = global_chunks_.load(std::memory_order_acquire);
        while (chunk) {
            std::byte* chunk_start = chunk->storage;
            std::byte* chunk_end = chunk_start + sizeof(T) * ChunkSize;
            std::byte* ptr_byte = reinterpret_cast<std::byte*>(ptr);

            if (ptr_byte >= chunk_start && ptr_byte < chunk_end) {
                // This is the correct chunk
                size_t remaining = chunk->allocated_count.fetch_sub(1, std::memory_order_acq_rel) - 1;

                if (remaining == 0) {
                    // Chunk is now empty, can be safely deleted after epoch passes
                    // For now, just add to free list
                    auto* node = reinterpret_cast<FreeNode*>(ptr);
                    node->next = free_list_;
                    free_list_ = node;
                } else {
                    // Add to thread-local free list
                    auto* node = reinterpret_cast<FreeNode*>(ptr);
                    node->next = free_list_;
                    free_list_ = node;
                }
                return;
            }
            chunk = chunk->next_chunk;
        }

#if ATOM_HAS_SPDLOG
        spdlog::warn("Attempted to deallocate pointer not from ThreadLocalPool");
#endif
    }

    /**
     * @brief Constructs an object in-place
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to constructed object
     */
    template<typename... Args>
    T* construct(Args&&... args) {
        T* ptr = allocate();
        try {
            new (ptr) T(std::forward<Args>(args)...);
            return ptr;
        } catch (...) {
            deallocate(ptr);
            throw;
        }
    }

    /**
     * @brief Destroys and deallocates an object
     * @param ptr Pointer to object to destroy
     */
    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
            deallocate(ptr);
        }
    }

    /**
     * @brief Gets allocation statistics
     * @return Tuple of (total_chunks, total_allocated, free_list_size)
     */
    std::tuple<size_t, size_t, size_t> get_stats() const {
        size_t chunk_count = 0;
        size_t total_allocated = 0;

        Chunk* chunk = global_chunks_.load(std::memory_order_acquire);
        while (chunk) {
            ++chunk_count;
            total_allocated += chunk->allocated_count.load(std::memory_order_relaxed);
            chunk = chunk->next_chunk;
        }

        size_t free_list_size = 0;
        FreeNode* node = free_list_;
        while (node) {
            ++free_list_size;
            node = node->next;
        }

        return std::make_tuple(chunk_count, total_allocated, free_list_size);
    }

    /**
     * @brief Destructor - cleans up all chunks
     */
    ~ThreadLocalPool() {
        Chunk* chunk = global_chunks_.load(std::memory_order_acquire);
        while (chunk) {
            Chunk* next = chunk->next_chunk;
            delete chunk;
            chunk = next;
        }
    }

    // Non-copyable and non-movable
    ThreadLocalPool(const ThreadLocalPool&) = delete;
    ThreadLocalPool& operator=(const ThreadLocalPool&) = delete;
    ThreadLocalPool(ThreadLocalPool&&) = delete;
    ThreadLocalPool& operator=(ThreadLocalPool&&) = delete;
};

// Static member definitions
template<typename T, size_t ChunkSize>
thread_local typename ThreadLocalPool<T, ChunkSize>::Chunk*
    ThreadLocalPool<T, ChunkSize>::current_chunk_ = nullptr;

template<typename T, size_t ChunkSize>
thread_local typename ThreadLocalPool<T, ChunkSize>::FreeNode*
    ThreadLocalPool<T, ChunkSize>::free_list_ = nullptr;

template<typename T, size_t ChunkSize>
thread_local size_t ThreadLocalPool<T, ChunkSize>::next_allocation_index_ = 0;

template<typename T, size_t ChunkSize>
EpochManager ThreadLocalPool<T, ChunkSize>::epoch_manager_;

} // namespace memory

// ============================================================================
// PERFORMANCE MONITORING
// ============================================================================

namespace monitoring {

/**
 * @brief Performance metrics for concurrency analysis
 */
struct ConcurrencyMetrics {
    alignas(64) std::atomic<uint64_t> lock_acquisitions{0};
    alignas(64) std::atomic<uint64_t> lock_contentions{0};
    alignas(64) std::atomic<uint64_t> spin_cycles{0};
    alignas(64) std::atomic<uint64_t> cache_hits{0};
    alignas(64) std::atomic<uint64_t> cache_misses{0};
    alignas(64) std::atomic<uint64_t> memory_allocations{0};
    alignas(64) std::atomic<uint64_t> memory_deallocations{0};
    alignas(64) std::atomic<uint64_t> epoch_advances{0};

    void reset() noexcept {
        lock_acquisitions.store(0, std::memory_order_relaxed);
        lock_contentions.store(0, std::memory_order_relaxed);
        spin_cycles.store(0, std::memory_order_relaxed);
        cache_hits.store(0, std::memory_order_relaxed);
        cache_misses.store(0, std::memory_order_relaxed);
        memory_allocations.store(0, std::memory_order_relaxed);
        memory_deallocations.store(0, std::memory_order_relaxed);
        epoch_advances.store(0, std::memory_order_relaxed);
    }

    double get_cache_hit_rate() const noexcept {
        uint64_t hits = cache_hits.load(std::memory_order_relaxed);
        uint64_t misses = cache_misses.load(std::memory_order_relaxed);
        uint64_t total = hits + misses;
        return total > 0 ? (100.0 * hits / total) : 0.0;
    }

    double get_contention_rate() const noexcept {
        uint64_t acquisitions = lock_acquisitions.load(std::memory_order_relaxed);
        uint64_t contentions = lock_contentions.load(std::memory_order_relaxed);
        return acquisitions > 0 ? (100.0 * contentions / acquisitions) : 0.0;
    }
};

/**
 * @brief Log entry for lock-free logging queue
 */
struct LogEntry {
    std::chrono::steady_clock::time_point timestamp;
    std::thread::id thread_id;
    std::string message;
    int level; // spdlog level

    LogEntry() = default;

    LogEntry(std::string msg, int log_level)
        : timestamp(std::chrono::steady_clock::now())
        , thread_id(std::this_thread::get_id())
        , message(std::move(msg))
        , level(log_level) {}
};

/**
 * @brief High-performance lock-free logger for concurrent systems
 */
class ConcurrentLogger {
private:
    static constexpr size_t QUEUE_SIZE = 8192;
    static constexpr size_t MAX_MESSAGE_SIZE = 1024;

    lockfree::LockFreeRingBuffer<LogEntry, QUEUE_SIZE> log_queue_;
    std::atomic<bool> running_{true};
    std::thread worker_thread_;

#if ATOM_HAS_SPDLOG
    std::shared_ptr<spdlog::logger> logger_;
#endif

    void worker_loop() {
        LogEntry entry;

        while (running_.load(std::memory_order_acquire)) {
            if (log_queue_.try_pop(entry)) {
#if ATOM_HAS_SPDLOG
                switch (entry.level) {
                    case 0: // trace
                        logger_->trace("[{}] {}", entry.thread_id, entry.message);
                        break;
                    case 1: // debug
                        logger_->debug("[{}] {}", entry.thread_id, entry.message);
                        break;
                    case 2: // info
                        logger_->info("[{}] {}", entry.thread_id, entry.message);
                        break;
                    case 3: // warn
                        logger_->warn("[{}] {}", entry.thread_id, entry.message);
                        break;
                    case 4: // error
                        logger_->error("[{}] {}", entry.thread_id, entry.message);
                        break;
                    case 5: // critical
                        logger_->critical("[{}] {}", entry.thread_id, entry.message);
                        break;
                }
#endif
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }

public:
    /**
     * @brief Constructs concurrent logger
     * @param logger_name Name for the logger
     */
    explicit ConcurrentLogger(const std::string& logger_name = "concurrent") {
#if ATOM_HAS_SPDLOG
        // Create async logger with rotating file sink
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/concurrent.log", 1024 * 1024 * 10, 3);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        logger_ = std::make_shared<spdlog::logger>(logger_name,
                                                  spdlog::sinks_init_list{file_sink, console_sink});
        logger_->set_level(spdlog::level::debug);
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        spdlog::register_logger(logger_);
#endif

        worker_thread_ = std::thread(&ConcurrentLogger::worker_loop, this);
    }

    /**
     * @brief Destructor - stops worker thread
     */
    ~ConcurrentLogger() {
        running_.store(false, std::memory_order_release);
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    /**
     * @brief Logs a message at trace level
     * @param message Message to log
     */
    void trace(const std::string& message) {
        log_queue_.try_push(LogEntry(message, 0));
    }

    /**
     * @brief Logs a message at debug level
     * @param message Message to log
     */
    void debug(const std::string& message) {
        log_queue_.try_push(LogEntry(message, 1));
    }

    /**
     * @brief Logs a message at info level
     * @param message Message to log
     */
    void info(const std::string& message) {
        log_queue_.try_push(LogEntry(message, 2));
    }

    /**
     * @brief Logs a formatted message at info level
     * @tparam Args Format argument types
     * @param format Format string
     * @param args Format arguments
     */
    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
#if ATOM_HAS_SPDLOG
        auto formatted = fmt::format(format, std::forward<Args>(args)...);
        log_queue_.try_push(LogEntry(formatted, 2));
#else
        log_queue_.try_push(LogEntry(format, 2));
#endif
    }

    /**
     * @brief Logs a message at warning level
     * @param message Message to log
     */
    void warn(const std::string& message) {
        log_queue_.try_push(LogEntry(message, 3));
    }

    /**
     * @brief Logs a message at error level
     * @param message Message to log
     */
    void error(const std::string& message) {
        log_queue_.try_push(LogEntry(message, 4));
    }

    /**
     * @brief Logs a message at critical level
     * @param message Message to log
     */
    void critical(const std::string& message) {
        log_queue_.try_push(LogEntry(message, 5));
    }

    /**
     * @brief Flushes all pending log messages
     */
    void flush() {
#if ATOM_HAS_SPDLOG
        logger_->flush();
#endif
    }

    // Non-copyable and non-movable
    ConcurrentLogger(const ConcurrentLogger&) = delete;
    ConcurrentLogger& operator=(const ConcurrentLogger&) = delete;
    ConcurrentLogger(ConcurrentLogger&&) = delete;
    ConcurrentLogger& operator=(ConcurrentLogger&&) = delete;
};

/**
 * @brief Performance monitor for concurrent systems
 */
class PerformanceMonitor {
private:
    ConcurrencyMetrics metrics_;
    ConcurrentLogger logger_;
    std::atomic<bool> monitoring_enabled_{true};
    std::thread monitor_thread_;

    static constexpr std::chrono::seconds REPORT_INTERVAL{5};

    void monitor_loop() {
        auto last_report = std::chrono::steady_clock::now();
        ConcurrencyMetrics last_metrics;

        while (monitoring_enabled_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto now = std::chrono::steady_clock::now();
            if (now - last_report >= REPORT_INTERVAL) {
                report_metrics(last_metrics);
                last_report = now;
                last_metrics = metrics_;
            }
        }
    }

    void report_metrics(const ConcurrencyMetrics& last_metrics) {
        auto current_acquisitions = metrics_.lock_acquisitions.load(std::memory_order_relaxed);
        auto current_contentions = metrics_.lock_contentions.load(std::memory_order_relaxed);
        auto current_allocations = metrics_.memory_allocations.load(std::memory_order_relaxed);

        auto delta_acquisitions = current_acquisitions - last_metrics.lock_acquisitions.load(std::memory_order_relaxed);
        auto delta_contentions = current_contentions - last_metrics.lock_contentions.load(std::memory_order_relaxed);
        auto delta_allocations = current_allocations - last_metrics.memory_allocations.load(std::memory_order_relaxed);

        logger_.info("Performance Report:");
        logger_.info("  Lock acquisitions: {} (delta: {})", current_acquisitions, delta_acquisitions);
        logger_.info("  Lock contentions: {} (delta: {})", current_contentions, delta_contentions);
        logger_.info("  Contention rate: {:.2f}%", metrics_.get_contention_rate());
        logger_.info("  Cache hit rate: {:.2f}%", metrics_.get_cache_hit_rate());
        logger_.info("  Memory allocations: {} (delta: {})", current_allocations, delta_allocations);
        logger_.info("  Epoch advances: {}", metrics_.epoch_advances.load(std::memory_order_relaxed));
    }

public:
    /**
     * @brief Constructs performance monitor
     */
    PerformanceMonitor() : logger_("performance_monitor") {
        monitor_thread_ = std::thread(&PerformanceMonitor::monitor_loop, this);
        logger_.info("Performance monitoring started");
    }

    /**
     * @brief Destructor - stops monitoring
     */
    ~PerformanceMonitor() {
        monitoring_enabled_.store(false, std::memory_order_release);
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
        logger_.info("Performance monitoring stopped");
    }

    /**
     * @brief Gets reference to metrics for updating
     * @return Reference to metrics
     */
    ConcurrencyMetrics& metrics() noexcept {
        return metrics_;
    }

    /**
     * @brief Gets reference to logger
     * @return Reference to logger
     */
    ConcurrentLogger& logger() noexcept {
        return logger_;
    }

    /**
     * @brief Enables or disables monitoring
     * @param enabled Whether to enable monitoring
     */
    void set_monitoring_enabled(bool enabled) noexcept {
        monitoring_enabled_.store(enabled, std::memory_order_release);
    }

    /**
     * @brief Resets all metrics
     */
    void reset_metrics() noexcept {
        metrics_.reset();
        logger_.info("Performance metrics reset");
    }

    // Non-copyable and non-movable
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;
    PerformanceMonitor(PerformanceMonitor&&) = delete;
    PerformanceMonitor& operator=(PerformanceMonitor&&) = delete;
};

/**
 * @brief Global performance monitor instance
 */
inline PerformanceMonitor& get_performance_monitor() {
    static PerformanceMonitor instance;
    return instance;
}

} // namespace monitoring

}  // namespace atom::extra

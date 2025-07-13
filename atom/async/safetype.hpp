#ifndef ATOM_ASYNC_SAFETYPE_HPP
#define ATOM_ASYNC_SAFETYPE_HPP

#include <atomic>
#include <concepts>  // C++20 concepts
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>  // C++20 ranges
#include <shared_mutex>
#include <span>
#include <vector>

#include "atom/error/exception.hpp"  // Assuming this provides THROW_RUNTIME_ERROR etc.

namespace atom::async {

// Concept for types that can be used in lock-free data structures with
// shared_ptr Requires nothrow destructibility for safety during concurrent
// cleanup.
template <typename T>
concept LockFreeSafe = std::is_nothrow_destructible_v<T>;

/**
 * @brief A lock-free stack implementation suitable for concurrent use.
 *
 * Uses std::atomic<std::shared_ptr<Node>> for lock-free operations on the head.
 * Note: While the head pointer updates are lock-free, the underlying shared_ptr
 * reference count operations involve atomic RMW operations which can still
 * introduce contention. For maximum performance in extreme contention,
 * pointer-based techniques with hazard pointers or RCU might be considered,
 * but this implementation leverages C++20 atomic shared_ptr.
 *
 * @tparam T Type of elements stored in the stack. Must satisfy LockFreeSafe.
 */
template <LockFreeSafe T>
class LockFreeStack {
private:
    struct Node {
        T value;
        std::atomic<std::shared_ptr<Node>> next{nullptr};

        explicit Node(T value_) noexcept(
            std::is_nothrow_move_constructible_v<T>)
            : value(std::move(value_)) {}
    };

    std::atomic<std::shared_ptr<Node>> head_{nullptr};
    // Approximate size is inherently racy in a lock-free structure,
    // but can be useful for heuristics. Use relaxed memory order.
    std::atomic<int> approximateSize_{0};

public:
    LockFreeStack() noexcept = default;

    /**
     * @brief Destroy the Lock Free Stack object.
     *
     * Cleanup of nodes is handled by shared_ptr reference counting when the
     * head_ is set to nullptr or nodes are popped. If threads are still
     * holding shared_ptrs to nodes (e.g., from pop() calls), cleanup might
     * be delayed until those shared_ptrs are released.
     */
    ~LockFreeStack() noexcept {
        head_.store(nullptr, std::memory_order_release);
        approximateSize_.store(0, std::memory_order_release);
    }

    // Non-copyable
    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;

    // Movable
    LockFreeStack(LockFreeStack&& other) noexcept
        : head_(other.head_.exchange(nullptr, std::memory_order_acq_rel)),
          approximateSize_(
              other.approximateSize_.exchange(0, std::memory_order_acq_rel)) {}

    LockFreeStack& operator=(LockFreeStack&& other) noexcept {
        if (this != &other) {
            // Clear current stack safely
            while (pop()) {
            }

            // Move from other using atomic exchange
            head_.store(
                other.head_.exchange(nullptr, std::memory_order_acq_rel),
                std::memory_order_release);
            approximateSize_.store(
                other.approximateSize_.exchange(0, std::memory_order_acq_rel),
                std::memory_order_release);
        }
        return *this;
    }

    /**
     * @brief Pushes a value onto the stack.
     *
     * @param value The value to push onto the stack.
     */
    void push(const T& value) noexcept(
        std::is_nothrow_copy_constructible_v<T>) {
        try {
            auto newNode = std::make_shared<Node>(value);
            push_node(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Cannot throw from a noexcept function.
        }
    }

    /**
     * @brief Pushes a value onto the stack using move semantics.
     *
     * @param value The value to move onto the stack.
     */
    void push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
        try {
            auto newNode = std::make_shared<Node>(std::move(value));
            push_node(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Cannot throw from a noexcept function.
        }
    }

    /**
     * @brief Attempts to pop the top value off the stack.
     *
     * @return std::optional<T> The popped value if stack is not empty,
     * otherwise nullopt.
     */
    auto pop() noexcept -> std::optional<T> {
        auto oldHead = head_.load(std::memory_order_acquire);
        std::shared_ptr<Node> newHead;

        while (oldHead) {
            // Load the next pointer of the current head. Relaxed order is fine
            // here as we only need the pointer value, not synchronization with
            // other threads modifying 'next'. The synchronization happens
            // via the CAS on 'head_'.
            newHead = oldHead->next.load(std::memory_order_relaxed);

            // Attempt to swap head_ from oldHead to newHead.
            // Use acq_rel: acquire semantics for loading head_ (ensures we see
            // the latest head), release semantics for storing newHead (ensures
            // subsequent loads see the new head).
            if (head_.compare_exchange_weak(oldHead, newHead,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
                approximateSize_.fetch_sub(1, std::memory_order_relaxed);
                return std::optional<T>{std::move(oldHead->value)};
            }
            // If CAS failed, oldHead is updated by compare_exchange_weak to the
            // current head, so the loop retries with the new head.
        }
        // Stack was empty or became empty during attempts.
        return std::nullopt;
    }

    /**
     * @brief Get the top value of the stack without removing it.
     *
     * @return std::optional<T> The top value if stack is not empty, otherwise
     * nullopt. Returns a copy of the value.
     */
    auto top() const noexcept -> std::optional<T> {
        // Acquire semantics to ensure we see the latest head and the data it
        // points to.
        auto currentHead = head_.load(std::memory_order_acquire);
        if (currentHead) {
            return std::optional<T>(currentHead->value);
        }
        return std::nullopt;
    }

    /**
     * @brief Check if the stack is empty.
     *
     * @return true If the stack is empty.
     * @return false If the stack has one or more elements.
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        // Acquire semantics to ensure we see the latest head.
        return head_.load(std::memory_order_acquire) == nullptr;
    }

    /**
     * @brief Get the approximate size of the stack.
     *
     * Note: This size is approximate due to the nature of lock-free operations.
     * Concurrent pushes and pops can make the reported size temporarily
     * inaccurate.
     *
     * @return int The approximate number of elements in the stack.
     */
    [[nodiscard]] auto size() const noexcept -> int {
        // Relaxed order is sufficient as this is an approximate size.
        return approximateSize_.load(std::memory_order_relaxed);
    }

private:
    /**
     * @brief Internal helper to push a pre-allocated node onto the stack.
     *
     * @param newNode The node to push.
     */
    void push_node(std::shared_ptr<Node> newNode) noexcept {
        // Load the current head. Relaxed order initially, as the CAS
        // will use acquire semantics on failure.
        std::shared_ptr<Node> expected = head_.load(std::memory_order_relaxed);

        do {
            // Set the new node's next pointer to the current head. Relaxed
            // order is fine here; the link is established before the CAS on
            // head_.
            newNode->next.store(expected, std::memory_order_relaxed);

            // Attempt to swap head_ from 'expected' to 'newNode'.
            // Use acq_rel: acquire semantics for loading head_ (ensures we see
            // the latest head if CAS fails), release semantics for storing
            // newNode (ensures subsequent loads see the new head).
        } while (!head_.compare_exchange_weak(expected, newNode,
                                              std::memory_order_acq_rel,
                                              std::memory_order_relaxed));

        approximateSize_.fetch_add(1, std::memory_order_relaxed);
    }
};

// Concept for types that can be used as keys and values in LockFreeHashTable
// Key must be hashable and equality comparable. Value must be default
// constructible and copyable.
template <typename T, typename U>
concept HashTableKeyValue = requires(T t, U u) {
    { std::hash<T>{}(t) } -> std::convertible_to<size_t>;
    { t == t } -> std::convertible_to<bool>;
    requires std::default_initializable<U>;
    requires std::copy_constructible<U>;
    { u = u } -> std::same_as<U&>;
};

/**
 * @brief A concurrent hash table implementation using linked lists for buckets.
 *
 * Uses std::atomic<std::shared_ptr<Node>> for lock-free operations on bucket
 * heads (insert). Find operations traverse the list without a lock. Erase
 * operations use a mutex per bucket to ensure safety during list modification.
 *
 * @tparam Key Type of keys. Must satisfy HashTableKeyValue requirements for
 * Key.
 * @tparam Value Type of values. Must satisfy HashTableKeyValue requirements for
 * Value.
 */
template <typename Key, typename Value>
    requires HashTableKeyValue<Key, Value>
class LockFreeHashTable {
private:
    struct Node {
        Key key;
        Value value;
        std::atomic<std::shared_ptr<Node>> next;

        Node(Key k,
             Value v) noexcept(std::is_nothrow_move_constructible_v<Key> &&
                               std::is_nothrow_move_constructible_v<Value>)
            : key(std::move(k)), value(std::move(v)), next(nullptr) {}
    };

    struct Bucket {
        std::atomic<std::shared_ptr<Node>> head;
        mutable std::mutex
            mutex_;  // Protects list traversal/modification for erase

        Bucket() noexcept : head(nullptr) {}

        // Find operation - traverses the list, not lock-free for the traversal
        auto find(const Key& key) const noexcept -> std::optional<Value> {
            auto node = head.load(std::memory_order_acquire);
            while (node) {
                if (node->key == key) {
                    return node->value;  // Return a copy
                }
                node = node->next.load(std::memory_order_acquire);
            }
            return std::nullopt;
        }

        // Insert operation - lock-free at the head of the bucket list
        // Returns true if inserted, false if key already exists
        bool insert(const Key& key, const Value& value) {
            // First, check if the key already exists to avoid unnecessary
            // allocation
            if (find(key)) {
                return false;  // Key already present
            }

            try {
                auto newNode = std::make_shared<Node>(key, value);
                std::shared_ptr<Node> expected =
                    head.load(std::memory_order_relaxed);

                do {
                    // Check again if key exists *before* attempting CAS
                    // This helps reduce contention on CAS if key is frequently
                    // checked/inserted by multiple threads.
                    auto currentNode = expected;
                    while (currentNode) {
                        if (currentNode->key == key) {
                            // Key was inserted by another thread concurrently
                            return false;
                        }
                        currentNode =
                            currentNode->next.load(std::memory_order_relaxed);
                    }

                    newNode->next.store(expected, std::memory_order_relaxed);

                } while (!head.compare_exchange_weak(
                    expected, newNode, std::memory_order_acq_rel,
                    std::memory_order_relaxed));

                return true;  // Successfully inserted
            } catch (const std::bad_alloc&) {
                // Handle allocation failure - cannot insert
                return false;
            }
        }

        // Erase operation - uses a mutex to protect list modification.
        // Not lock-free, but thread-safe.
        bool erase(const Key& key) noexcept {
            // Acquire lock for safe traversal and modification
            std::lock_guard<std::mutex> lock(mutex_);

            auto currentNode = head.load(std::memory_order_acquire);
            std::shared_ptr<Node> prevNode = nullptr;

            while (currentNode) {
                if (currentNode->key == key) {
                    // Found the node to delete
                    if (!prevNode) {
                        // Removing head node
                        // Atomically update head
                        head.store(
                            currentNode->next.load(std::memory_order_relaxed),
                            std::memory_order_release);
                    } else {
                        // Removing non-head node
                        // Atomically update prevNode's next
                        prevNode->next.store(
                            currentNode->next.load(std::memory_order_relaxed),
                            std::memory_order_release);
                    }
                    // shared_ptr handles deletion of currentNode when it goes
                    // out of scope
                    return true;  // Successfully removed
                }

                // Move to the next node
                prevNode = currentNode;
                currentNode = currentNode->next.load(std::memory_order_acquire);
            }
            return false;  // Key not found
        }
    };

    std::vector<std::unique_ptr<Bucket>> buckets_;
    std::hash<Key> hasher_;
    // Approximate size, use relaxed memory order
    std::atomic<size_t> size_{0};

    auto getBucket(const Key& key) const noexcept -> Bucket& {
        // Use std::hash and modulo for bucket index.
        // Ensure index is within bounds.
        size_t bucketIndex = hasher_(key) % buckets_.size();
        return *buckets_[bucketIndex];
    }

public:
    /**
     * @brief Construct a new Concurrent Hash Table.
     *
     * @param num_buckets The number of buckets to use. Must be at least 1.
     */
    explicit LockFreeHashTable(size_t num_buckets = 16)
        : buckets_(std::max(num_buckets, size_t(1))) {
        for (size_t i = 0; i < buckets_.size(); ++i) {
            buckets_[i] = std::make_unique<Bucket>();
        }
    }

    // Support for range constructors (C++20)
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>,
                                     std::pair<Key, Value>>
    explicit LockFreeHashTable(R&& range, size_t num_buckets = 16)
        : LockFreeHashTable(num_buckets) {
        for (auto&& pair : range) {
            insert(pair.first, pair.second);
        }
    }

    // Non-copyable, non-movable due to unique_ptr in vector and complex state
    LockFreeHashTable(const LockFreeHashTable&) = delete;
    LockFreeHashTable& operator=(const LockFreeHashTable&) = delete;
    LockFreeHashTable(LockFreeHashTable&&) = delete;
    LockFreeHashTable& operator=(LockFreeHashTable&&) = delete;

    /**
     * @brief Find a value by key.
     *
     * @param key The key to search for.
     * @return std::optional<Value> A copy of the value if found, otherwise
     * nullopt.
     */
    auto find(const Key& key) const noexcept -> std::optional<Value> {
        return getBucket(key).find(key);
    }

    /**
     * @brief Insert a key-value pair.
     *
     * @param key The key to insert.
     * @param value The value to insert.
     * @return true If the key-value pair was successfully inserted (key did not
     * exist).
     * @return false If the key already existed or allocation failed.
     */
    bool insert(const Key& key, const Value& value) {
        bool inserted = getBucket(key).insert(key, value);
        if (inserted) {
            size_.fetch_add(1, std::memory_order_relaxed);
        }
        return inserted;
    }

    /**
     * @brief Erase a key-value pair by key.
     *
     * Note: This operation uses a mutex per bucket and is not lock-free.
     *
     * @param key The key to erase.
     * @return true If the key was found and erased.
     * @return false If the key was not found.
     */
    bool erase(const Key& key) noexcept {
        bool result = getBucket(key).erase(key);
        if (result) {
            size_.fetch_sub(1, std::memory_order_relaxed);
        }
        return result;
    }

    /**
     * @brief Check if the hash table is empty (approximately).
     *
     * @return true If the approximate size is 0.
     * @return false Otherwise.
     */
    [[nodiscard]] auto empty() const noexcept -> bool { return size() == 0; }

    /**
     * @brief Get the approximate size of the hash table.
     *
     * Note: This size is approximate due to the nature of concurrent
     * operations.
     *
     * @return size_t The approximate number of elements.
     */
    [[nodiscard]] auto size() const noexcept -> size_t {
        return size_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Clear all elements from the hash table.
     *
     * Note: This operation is not lock-free. It iterates through buckets
     * and atomically exchanges the head pointers to nullptr.
     */
    void clear() noexcept {
        for (const auto& bucket : buckets_) {
            // Atomically set bucket head to nullptr.
            // acq_rel ensures this is visible after clearing starts.
            [[maybe_unused]] auto oldHead =
                bucket->head.exchange(nullptr, std::memory_order_acq_rel);
            // shared_ptr handles the deallocation of the old list nodes.
        }
        // Set approximate size to 0. Release semantics ensures this is visible
        // after clearing starts.
        size_.store(0, std::memory_order_release);
    }
};

// C++20 concept for thread-safe vector elements
// Requires nothrow move constructibility and destructibility for safe handling
// of elements during resize and destruction.
template <typename T>
concept ThreadSafeVectorElem = std::is_nothrow_move_constructible_v<T> &&
                               std::is_nothrow_destructible_v<T>;

/**
 * @brief A thread-safe vector implementation.
 *
 * Uses std::atomic<T>[] for atomic access to individual elements and
 * std::shared_mutex for protecting resize operations. Push/Pop operations
 * use lock-free techniques on the size counter.
 *
 * @tparam T Type of elements. Must satisfy ThreadSafeVectorElem.
 */
template <ThreadSafeVectorElem T>
class ThreadSafeVector {
    // Use unique_ptr for dynamic array of atomic elements
    std::unique_ptr<std::atomic<T>[]> data_;
    std::atomic<size_t> capacity_;
    std::atomic<size_t> size_;
    mutable std::shared_mutex resize_mutex_;  // Protects resize operations

    // Internal resize function, must be called with resize_mutex_ locked
    // exclusively
    void resize() {
        // Assumes resize_mutex_ is already locked exclusively by the caller

        size_t oldCapacity = capacity_.load(std::memory_order_relaxed);
        size_t currentSize = size_.load(
            std::memory_order_relaxed);  // Use relaxed as mutex provides sync

        // Calculate new capacity, ensure it's at least 1 if currentSize is 0
        size_t newCapacity = std::max(oldCapacity * 2, size_t(1));
        // Ensure new capacity is at least current size if resize was triggered
        // by pushBack
        newCapacity = std::max(newCapacity, currentSize > 0 ? currentSize : 1);

        // Avoid unnecessary resize if capacity is already sufficient
        if (newCapacity <= oldCapacity) {
            return;
        }

        try {
            // Allocate new data array
            auto newData = std::make_unique<std::atomic<T>[]>(newCapacity);

            // Copy/Move elements from old array to new array
            for (size_t i = 0; i < currentSize; ++i) {
                // Atomically load from old array and store to new array
                // Relaxed order is sufficient here as the mutex provides the
                // necessary synchronization for the array contents themselves.
                newData[i].store(data_[i].load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
            }

            // Atomically swap the data pointers.
            // Release semantics for the store to data_ ensures the new array
            // contents are visible before the pointer update.
            // Acquire semantics for the load from data_ (implicit in swap)
            // ensures we see the old array correctly before swapping.
            data_.swap(newData);
            // Update capacity. Release semantics ensures the new capacity is
            // visible after the data swap.
            capacity_.store(newCapacity, std::memory_order_release);

            // The old data (pointed to by newData after swap) will be
            // deallocated when newData goes out of scope.
        } catch (const std::bad_alloc& e) {
            // Handle allocation failure during resize.
            // Rethrow as a runtime error.
            THROW_RUNTIME_ERROR("Failed to resize ThreadSafeVector: " +
                                std::string(e.what()));
        }
    }

public:
    /**
     * @brief Construct a new Thread Safe Vector.
     *
     * @param initial_capacity The initial capacity of the vector. Must be at
     * least 1.
     */
    explicit ThreadSafeVector(size_t initial_capacity = 16)
        : capacity_(std::max(initial_capacity, size_t(1))), size_(0) {
        try {
            // Allocate initial data array
            data_ = std::make_unique<std::atomic<T>[]>(
                capacity_.load(std::memory_order_relaxed));
        } catch (const std::bad_alloc& e) {
            // Handle allocation failure
            THROW_RUNTIME_ERROR(
                "Failed to allocate initial memory for ThreadSafeVector");
        }
    }

    // Support for range constructors (C++20)
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, T>
    explicit ThreadSafeVector(R&& range, size_t initial_capacity = 16)
        : ThreadSafeVector(initial_capacity) {
        for (auto&& item : range) {
            pushBack(item);
        }
    }

    // Non-copyable, non-movable due to unique_ptr and mutex
    ThreadSafeVector(const ThreadSafeVector&) = delete;
    ThreadSafeVector& operator=(const ThreadSafeVector&) = delete;
    ThreadSafeVector(ThreadSafeVector&&) = delete;
    ThreadSafeVector& operator=(ThreadSafeVector&&) = delete;

    /**
     * @brief Add an element to the end of the vector.
     *
     * May trigger a resize if capacity is insufficient.
     *
     * @param value The value to add.
     * @throws atom::error::runtime_error if resize fails.
     */
    void pushBack(const T& value) {
        size_t currentSize = size_.load(std::memory_order_relaxed);
        while (true) {
            // Check if there is enough capacity
            if (currentSize < capacity_.load(std::memory_order_relaxed)) {
                // Attempt to atomically increment size and claim the slot
                // acq_rel semantics for success: acquire for reading
                // currentSize, release for making the new size visible.
                if (size_.compare_exchange_weak(currentSize, currentSize + 1,
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed)) {
                    // Successfully claimed slot 'currentSize'. Store the value.
                    // Release semantics ensures the value is written before
                    // the size increment becomes visible.
                    data_[currentSize].store(value, std::memory_order_release);
                    return;  // Element added successfully
                }
                // If CAS failed, currentSize is updated by
                // compare_exchange_weak to the new size, loop retries.
            } else {
                // Capacity is full, need to resize.
                // Acquire exclusive lock for resize.
                std::unique_lock lock(resize_mutex_);
                // Re-check size and capacity under the lock, as another thread
                // might have resized while we were waiting for the lock.
                if (size_.load(std::memory_order_relaxed) <
                    capacity_.load(std::memory_order_relaxed)) {
                    // Another thread resized, capacity is now sufficient.
                    // Release the lock and retry the pushBack loop.
                    lock.unlock();
                    currentSize =
                        size_.load(std::memory_order_relaxed);  // Reload size
                    continue;
                }
                // Still need to resize.
                resize();  // This might throw bad_alloc
                // After successful resize, release the lock and retry the
                // pushBack loop.
                lock.unlock();
                currentSize =
                    size_.load(std::memory_order_relaxed);  // Reload size
            }
        }
    }

    /**
     * @brief Add an element to the end of the vector using move semantics.
     *
     * May trigger a resize if capacity is insufficient.
     *
     * @param value The value to move.
     * @throws atom::error::runtime_error if resize fails (only if T's move
     * constructor throws).
     */
    void pushBack(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
        size_t currentSize = size_.load(std::memory_order_relaxed);
        while (true) {
            if (currentSize < capacity_.load(std::memory_order_relaxed)) {
                if (size_.compare_exchange_weak(currentSize, currentSize + 1,
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed)) {
                    data_[currentSize].store(std::move(value),
                                             std::memory_order_release);
                    return;
                }
            } else {
                // Capacity is full, need to resize.
                std::unique_lock lock(resize_mutex_);
                if (size_.load(std::memory_order_relaxed) <
                    capacity_.load(std::memory_order_relaxed)) {
                    lock.unlock();
                    currentSize = size_.load(std::memory_order_relaxed);
                    continue;
                }
                try {
                    resize();  // This might throw bad_alloc
                } catch (const std::exception& e) {
                    // If resize fails, we cannot add the element.
                    // Since this is noexcept, we cannot rethrow.
                    return;  // Return without adding the element
                }
                lock.unlock();
                currentSize = size_.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Remove and return the last element.
     *
     * @return std::optional<T> The popped value if vector is not empty,
     * otherwise nullopt.
     */
    auto popBack() noexcept -> std::optional<T> {
        size_t currentSize = size_.load(std::memory_order_relaxed);
        while (currentSize > 0) {
            // Attempt to atomically decrement size
            // acq_rel semantics for success: acquire for reading currentSize,
            // release for making the new size visible.
            if (size_.compare_exchange_weak(currentSize, currentSize - 1,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
                // Successfully claimed slot 'currentSize - 1'. Load the value.
                // Acquire semantics ensures we read the value after the size
                // decrement is visible.
                return data_[currentSize - 1].load(std::memory_order_acquire);
            }
            // If CAS failed, currentSize is updated by compare_exchange_weak,
            // loop retries.
        }
        // Vector was empty or became empty during attempts.
        return std::nullopt;
    }

    /**
     * @brief Get a copy of the element at a specific index.
     *
     * @param index The index of the element.
     * @return T A copy of the element.
     * @throws atom::error::out_of_range if index is out of bounds.
     */
    auto at(size_t index) const -> T {
        // Acquire semantics to ensure we see the latest size and data.
        if (index >= size_.load(std::memory_order_acquire)) {
            THROW_OUT_OF_RANGE("Index out of range in ThreadSafeVector::at()");
        }
        // Acquire semantics to read the element value.
        return data_[index].load(std::memory_order_acquire);
    }

    /**
     * @brief Attempt to get a copy of the element at a specific index without
     * throwing.
     *
     * @param index The index of the element.
     * @return std::optional<T> A copy of the element if index is valid,
     * otherwise nullopt.
     */
    auto try_at(size_t index) const noexcept -> std::optional<T> {
        // Acquire semantics to ensure we see the latest size.
        if (index >= size_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        // Acquire semantics to read the element value.
        return data_[index].load(std::memory_order_acquire);
    }

    /**
     * @brief Check if the vector is empty.
     *
     * @return true If the vector is empty.
     * @return false Otherwise.
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        // Acquire semantics to ensure we see the latest size.
        return size_.load(std::memory_order_acquire) == 0;
    }

    /**
     * @brief Get the current size of the vector.
     *
     * @return size_t The current number of elements.
     */
    [[nodiscard]] auto getSize() const noexcept -> size_t {
        // Acquire semantics to ensure we see the latest size.
        return size_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get the current capacity of the vector.
     *
     * @return size_t The current allocated capacity.
     */
    [[nodiscard]] auto getCapacity() const noexcept -> size_t {
        // Acquire semantics to ensure we see the latest capacity.
        return capacity_.load(std::memory_order_acquire);
    }

    /**
     * @brief Clear the vector, setting size to 0.
     *
     * Does not deallocate memory. Note: Elements are not destructed by clear.
     * This clear only logically empties the vector. If T requires explicit
     * cleanup, a different approach is needed.
     */
    void clear() noexcept {
        // Release semantics ensures that subsequent reads see the size as 0.
        size_.store(0, std::memory_order_release);
    }

    /**
     * @brief Reduce capacity to fit the current size.
     *
     * Acquires an exclusive lock.
     */
    void shrinkToFit() {
        // Acquire exclusive lock as this modifies the underlying data array.
        std::unique_lock lock(resize_mutex_);

        size_t currentSize = size_.load(std::memory_order_relaxed);
        size_t currentCapacity = capacity_.load(std::memory_order_relaxed);

        // Target capacity is current size, but at least 1 if size is 0.
        size_t targetCapacity = currentSize > 0 ? currentSize : 1;

        if (targetCapacity >= currentCapacity) {
            return;  // Already at optimal size or need to grow
        }

        try {
            // Allocate new data array with target capacity
            auto newData = std::make_unique<std::atomic<T>[]>(targetCapacity);

            // Copy/Move elements to the new array
            for (size_t i = 0; i < currentSize; ++i) {
                // Relaxed order is sufficient under the mutex.
                newData[i].store(data_[i].load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
            }

            // Atomically swap data pointers and update capacity.
            // Release semantics for stores ensures visibility.
            data_.swap(newData);
            capacity_.store(targetCapacity, std::memory_order_release);

            // Old data deallocated when newData goes out of scope.
        } catch (const std::bad_alloc& e) {
            // Ignore errors during shrink - it's just an optimization.
        }
    }

    /**
     * @brief Get a copy of the first element.
     *
     * @return T A copy of the first element.
     * @throws atom::error::out_of_range if vector is empty.
     */
    auto front() const -> T {
        // Acquire semantics for size check.
        if (empty()) {
            THROW_OUT_OF_RANGE("Vector is empty in ThreadSafeVector::front()");
        }
        // Acquire semantics to read the element.
        return data_[0].load(std::memory_order_acquire);
    }

    /**
     * @brief Attempt to get a copy of the first element without throwing.
     *
     * @return std::optional<T> A copy of the first element if vector is not
     * empty, otherwise nullopt.
     */
    auto try_front() const noexcept -> std::optional<T> {
        // Acquire semantics for size check.
        if (empty()) {
            return std::nullopt;
        }
        // Acquire semantics to read the element.
        return data_[0].load(std::memory_order_acquire);
    }

    /**
     * @brief Get a copy of the last element.
     *
     * @return T A copy of the last element.
     * @throws atom::error::out_of_range if vector is empty.
     */
    auto back() const -> T {
        // Acquire semantics for size check.
        size_t currentSize = size_.load(std::memory_order_acquire);
        if (currentSize == 0) {
            THROW_OUT_OF_RANGE("Vector is empty in ThreadSafeVector::back()");
        }
        // Acquire semantics to read the element.
        return data_[currentSize - 1].load(std::memory_order_acquire);
    }

    /**
     * @brief Attempt to get a copy of the last element without throwing.
     *
     * @return std::optional<T> A copy of the last element if vector is not
     * empty, otherwise nullopt.
     */
    auto try_back() const noexcept -> std::optional<T> {
        // Acquire semantics for size check.
        size_t currentSize = size_.load(std::memory_order_acquire);
        if (currentSize == 0) {
            return std::nullopt;
        }
        // Acquire semantics to read the element.
        return data_[currentSize - 1].load(std::memory_order_acquire);
    }

    /**
     * @brief Get a copy of the element at a specific index (bounds checked).
     *
     * Same as at().
     *
     * @param index The index of the element.
     * @return T A copy of the element.
     * @throws atom::error::out_of_range if index is out of bounds.
     */
    auto operator[](size_t index) const -> T { return at(index); }

    // C++20: Support for std::span view of the data.
    // Returns a span of the underlying atomic elements.
    // The caller must use atomic loads/stores when accessing elements via the
    // span. The span is only valid as long as the ThreadSafeVector is not
    // resized. Holding a shared_lock while using the span is recommended to
    // prevent resize.
    /**
     * @brief Get a read-only span view of the underlying atomic data.
     *
     * The returned span points to the internal std::atomic<T>[] array.
     * Accessing elements via the span requires using atomic operations (e.g.,
     * .load()). The span is invalidated if the vector is resized. It is
     * recommended to hold a std::shared_lock on the vector's internal mutex
     * while using the span to prevent concurrent resizing.
     *
     * @return std::span<const std::atomic<T>> A span view of the data.
     */
    auto get_span() const -> std::span<const std::atomic<T>> {
        // Load size and data pointer atomically. Acquire semantics ensures
        // we see the latest state before creating the span.
        size_t currentSize = size_.load(std::memory_order_acquire);
        std::atomic<T>* dataPtr = data_.get();  // Get raw pointer

        // Return a span pointing to the raw atomic array.
        // The caller *must* ensure the vector is not resized while using this
        // span. A shared_lock held by the caller is the way to do this.
        return std::span<const std::atomic<T>>(dataPtr, currentSize);
    }
};

// C++20 concept for lock-free list elements
// Requires nothrow move constructibility and destructibility for safe handling
// with shared_ptr in a lock-free context.
template <typename T>
concept LockFreeListElem = std::is_nothrow_move_constructible_v<T> &&
                           std::is_nothrow_destructible_v<T>;

/**
 * @brief A lock-free singly linked list implementation.
 *
 * Supports lock-free pushFront and popFront operations using
 * std::atomic<std::shared_ptr<Node>> for the head pointer.
 * Note: Similar to LockFreeStack, shared_ptr reference counting can introduce
 * contention under high concurrency.
 *
 * @tparam T Type of elements. Must satisfy LockFreeListElem.
 */
template <LockFreeListElem T>
class LockFreeList {
private:
    struct Node {
        T value;
        std::atomic<std::shared_ptr<Node>> next;

        explicit Node(const T& val) noexcept(
            std::is_nothrow_copy_constructible_v<T>)
            : value(val), next(nullptr) {}

        explicit Node(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>)
            : value(std::move(val)), next(nullptr) {}
    };

    std::atomic<std::shared_ptr<Node>> head_{nullptr};
    // Approximate size, use relaxed memory order
    std::atomic<size_t> size_{0};

public:
    LockFreeList() noexcept = default;

    /**
     * @brief Destroy the Lock Free List.
     *
     * Cleanup is handled by shared_ptr reference counting.
     */
    ~LockFreeList() noexcept = default;  // Smart pointers handle cleanup

    // Non-copyable
    LockFreeList(const LockFreeList&) = delete;
    LockFreeList& operator=(const LockFreeList&) = delete;

    // Movable
    LockFreeList(LockFreeList&& other) noexcept
        : head_(other.head_.exchange(nullptr, std::memory_order_acq_rel)),
          size_(other.size_.exchange(0, std::memory_order_acq_rel)) {}

    LockFreeList& operator=(LockFreeList&& other) noexcept {
        if (this != &other) {
            // Clear current list safely
            while (popFront()) {
            }
            // Move from other using atomic exchange
            head_.store(
                other.head_.exchange(nullptr, std::memory_order_acq_rel),
                std::memory_order_release);
            size_.store(other.size_.exchange(0, std::memory_order_acq_rel),
                        std::memory_order_release);
        }
        return *this;
    }

    /**
     * @brief Add an element to the front of the list.
     *
     * @param value The value to add.
     */
    void pushFront(const T& value) {
        try {
            auto newNode = std::make_shared<Node>(value);
            pushNodeFront(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Log memory allocation failure
        }
    }

    /**
     * @brief Add an element to the front of the list using move semantics.
     *
     * @param value The value to move.
     */
    void pushFront(T&& value) noexcept(
        std::is_nothrow_move_constructible_v<T>) {
        try {
            auto newNode = std::make_shared<Node>(std::move(value));
            pushNodeFront(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Log memory allocation failure
        }
    }

    /**
     * @brief Remove and return the first element.
     *
     * @return std::optional<T> The popped value if list is not empty, otherwise
     * nullopt.
     */
    auto popFront() noexcept -> std::optional<T> {
        auto oldHead = head_.load(std::memory_order_acquire);
        std::shared_ptr<Node> newHead;

        while (oldHead) {
            // Load next pointer with relaxed order, sync via CAS on head_
            newHead = oldHead->next.load(std::memory_order_relaxed);
            // Attempt to swing head_ from oldHead to newHead
            // acq_rel semantics for CAS
            if (head_.compare_exchange_weak(oldHead, newHead,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
                size_.fetch_sub(1, std::memory_order_relaxed);
                return std::optional<T>{std::move(oldHead->value)};
            }
            // If CAS failed, oldHead is updated, loop retries.
        }
        // List was empty or became empty.
        return std::nullopt;
    }

    /**
     * @brief Get a copy of the first element without removing it.
     *
     * @return std::optional<T> A copy of the first element if list is not
     * empty, otherwise nullopt.
     */
    auto front() const noexcept -> std::optional<T> {
        // Acquire semantics to see the latest head and data.
        auto currentHead = head_.load(std::memory_order_acquire);
        if (currentHead) {
            return std::optional<T>(currentHead->value);
        }
        return std::nullopt;
    }

    /**
     * @brief Check if the list is empty.
     *
     * @return true If the list is empty.
     * @return false If the list has one or more elements.
     */
    [[nodiscard]] bool empty() const noexcept {
        // Acquire semantics to see the latest head.
        return head_.load(std::memory_order_acquire) == nullptr;
    }

    /**
     * @brief Get the approximate size of the list.
     *
     * Note: This size is approximate.
     *
     * @return size_t The approximate number of elements.
     */
    [[nodiscard]] auto size() const noexcept -> size_t {
        // Relaxed order for approximate size.
        return size_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Clear the list.
     *
     * Atomically sets the head to nullptr. Cleanup handled by shared_ptr.
     */
    void clear() noexcept {
        // Atomically set head to nullptr. acq_rel ensures visibility.
        [[maybe_unused]] auto oldHead =
            head_.exchange(nullptr, std::memory_order_acq_rel);
        // Set approximate size to 0. Release ensures visibility.
        size_.store(0, std::memory_order_release);
        // shared_ptr handles deallocation of the old list nodes.
    }

private:
    /**
     * @brief Internal helper to push a pre-allocated node onto the list front.
     *
     * @param newNode The node to push.
     */
    void pushNodeFront(std::shared_ptr<Node> newNode) noexcept {
        // Load the current head. Relaxed order initially.
        std::shared_ptr<Node> expected = head_.load(std::memory_order_relaxed);

        do {
            // Set the new node's next pointer to the current head. Relaxed
            // order.
            newNode->next.store(expected, std::memory_order_relaxed);

            // Attempt to swap head_ from 'expected' to 'newNode'.
            // acq_rel semantics for CAS.
        } while (!head_.compare_exchange_weak(expected, newNode,
                                              std::memory_order_acq_rel,
                                              std::memory_order_relaxed));

        size_.fetch_add(1, std::memory_order_relaxed);
    }
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_SAFETYPE_HPP

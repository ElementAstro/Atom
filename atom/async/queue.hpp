/*
 * queue.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-13

Description: A simple thread safe queue

**************************************************/

#ifndef ATOM_ASYNC_QUEUE_HPP
#define ATOM_ASYNC_QUEUE_HPP

#include <algorithm>
#include <atomic>
#include <concepts>
#include <condition_variable>
#include <execution>
#include <functional>
#include <future>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>  // For read-write lock
#include <span>
#include <stdexcept>
#include <thread>  // For yield in spin lock
#include <type_traits>
#include <unordered_map>
#include <vector>

// Boost lockfree dependency
#ifdef ATOM_USE_LOCKFREE_QUEUE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

namespace atom::async {

// High-performance lock implementations

/**
 * @brief High-performance spin lock implementation
 *
 * Uses atomic operations for low-contention scenarios.
 * Spins with exponential backoff for better performance.
 */
class SpinLock {
public:
    SpinLock() = default;
    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    void lock() noexcept {
        std::uint32_t backoff = 1;
        while (m_lock.test_and_set(std::memory_order_acquire)) {
            // Exponential backoff strategy
            for (std::uint32_t i = 0; i < backoff; ++i) {
// Pause instruction to reduce power consumption and improve performance
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
                _mm_pause();
#elif defined(__arm__) || defined(__aarch64__)
                __asm__ __volatile__("yield" ::: "memory");
#else
                std::this_thread::yield();
#endif
            }

            // Increase backoff to reduce contention, with upper limit
            if (backoff < 1024) {
                backoff *= 2;
            } else {
                // After significant spinning, yield to prevent CPU hogging
                std::this_thread::yield();
            }
        }
    }

    bool try_lock() noexcept {
        return !m_lock.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept { m_lock.clear(std::memory_order_release); }

private:
    std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
};

/**
 * @brief Read-write lock for concurrent read access
 *
 * Allows multiple readers to access simultaneously, but exclusive write access.
 * Uses std::shared_mutex internally for reader-writer pattern.
 */
class SharedMutex {
public:
    SharedMutex() = default;
    SharedMutex(const SharedMutex&) = delete;
    SharedMutex& operator=(const SharedMutex&) = delete;

    void lock() noexcept { m_mutex.lock(); }

    void unlock() noexcept { m_mutex.unlock(); }

    void lock_shared() noexcept { m_mutex.lock_shared(); }

    void unlock_shared() noexcept { m_mutex.unlock_shared(); }

    bool try_lock() noexcept { return m_mutex.try_lock(); }

    bool try_lock_shared() noexcept { return m_mutex.try_lock_shared(); }

private:
    std::shared_mutex m_mutex;
};

/**
 * @brief Hybrid mutex with adaptive lock strategy
 *
 * Combines spinning and blocking approaches.
 * Spins for a short period before falling back to blocking.
 */
class HybridMutex {
public:
    HybridMutex() = default;
    HybridMutex(const HybridMutex&) = delete;
    HybridMutex& operator=(const HybridMutex&) = delete;

    void lock() noexcept {
        // First try spinning for a short time
        constexpr int SPIN_COUNT = 4000;
        for (int i = 0; i < SPIN_COUNT; ++i) {
            if (try_lock()) {
                return;
            }

// Pause to reduce CPU consumption and bus contention
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
            _mm_pause();
#elif defined(__arm__) || defined(__aarch64__)
            __asm__ __volatile__("yield" ::: "memory");
#else
            // No specific CPU hint, use compiler barrier
            std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
        }

        // If spinning didn't succeed, fall back to blocking mutex
        m_mutex.lock();
        m_isThreadLocked.store(true, std::memory_order_relaxed);
    }

    bool try_lock() noexcept {
        // Try to acquire through atomic flag first
        if (!m_spinLock.test_and_set(std::memory_order_acquire)) {
            // Make sure we're not already locked by the mutex
            if (m_isThreadLocked.load(std::memory_order_relaxed)) {
                m_spinLock.clear(std::memory_order_release);
                return false;
            }
            return true;
        }
        return false;
    }

    void unlock() noexcept {
        // If locked by the mutex, unlock it
        if (m_isThreadLocked.load(std::memory_order_relaxed)) {
            m_isThreadLocked.store(false, std::memory_order_relaxed);
            m_mutex.unlock();
        } else {
            // Otherwise just clear the spin lock
            m_spinLock.clear(std::memory_order_release);
        }
    }

private:
    std::atomic_flag m_spinLock = ATOMIC_FLAG_INIT;
    std::mutex m_mutex;
    std::atomic<bool> m_isThreadLocked{false};
};

// Forward declarations of lock guards for custom mutexes
template <typename Mutex>
class lock_guard {
public:
    explicit lock_guard(Mutex& mutex) : m_mutex(mutex) { m_mutex.lock(); }

    ~lock_guard() { m_mutex.unlock(); }

    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;

private:
    Mutex& m_mutex;
};

template <typename Mutex>
class shared_lock {
public:
    explicit shared_lock(Mutex& mutex) : m_mutex(mutex) {
        m_mutex.lock_shared();
    }

    ~shared_lock() { m_mutex.unlock_shared(); }

    shared_lock(const shared_lock&) = delete;
    shared_lock& operator=(const shared_lock&) = delete;

private:
    Mutex& m_mutex;
};

// Concepts for improved compile-time type checking
template <typename T>
concept Movable = std::move_constructible<T> && std::assignable_from<T&, T>;

template <typename T, typename U>
concept ExtractableWith = requires(T t, U u) {
    { u(t) } -> std::convertible_to<bool>;
};

// Main thread-safe queue implementation with high-performance locks
template <Movable T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;  // Prevent copying
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue(ThreadSafeQueue&&) noexcept = default;
    ThreadSafeQueue& operator=(ThreadSafeQueue&&) noexcept = default;
    ~ThreadSafeQueue() noexcept {
        try {
            destroy();
        } catch (...) {
            // Ensure no exceptions escape destructor
        }
    }

    /**
     * @brief Add an element to the queue
     * @param element Element to be added
     * @throws std::bad_alloc if memory allocation fails
     */
    void put(T element) noexcept(std::is_nothrow_move_constructible_v<T>) {
        try {
            {
                lock_guard lock(m_mutex);
                m_queue_.push(std::move(element));
            }
            m_conditionVariable_.notify_one();
        } catch (const std::exception&) {
            // Error handling
        }
    }

    /**
     * @brief Take an element from the queue
     * @return Optional containing the element or nothing if queue is being
     * destroyed
     */
    [[nodiscard]] auto take() -> std::optional<T> {
        std::unique_lock<HybridMutex> lock(m_mutex);
        // Avoid spurious wakeups
        while (!m_mustReturnNullptr_ && m_queue_.empty()) {
            m_conditionVariable_.wait(lock);
        }

        if (m_mustReturnNullptr_ || m_queue_.empty()) {
            return std::nullopt;
        }

        // Use move semantics to directly construct optional, reducing one move
        // operation
        std::optional<T> ret{std::move(m_queue_.front())};
        m_queue_.pop();
        return ret;
    }

    /**
     * @brief Destroy the queue and return remaining elements
     * @return Queue containing all remaining elements
     */
    [[nodiscard]] auto destroy() noexcept -> std::queue<T> {
        {
            lock_guard lock(m_mutex);
            m_mustReturnNullptr_ = true;
        }
        m_conditionVariable_.notify_all();

        std::queue<T> result;
        {
            lock_guard lock(m_mutex);
            std::swap(result, m_queue_);
        }
        return result;
    }

    /**
     * @brief Get the size of the queue
     * @return Current size of the queue
     */
    [[nodiscard]] auto size() const noexcept -> size_t {
        lock_guard lock(m_mutex);
        return m_queue_.size();
    }

    /**
     * @brief Check if the queue is empty
     * @return True if queue is empty, false otherwise
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        lock_guard lock(m_mutex);
        return m_queue_.empty();
    }

    /**
     * @brief Clear all elements from the queue
     */
    void clear() noexcept {
        lock_guard lock(m_mutex);
        std::queue<T> empty;
        std::swap(m_queue_, empty);
    }

    /**
     * @brief Get the front element without removing it
     * @return Optional containing the front element or nothing if queue is
     * empty
     */
    [[nodiscard]] auto front() const -> std::optional<T> {
        lock_guard lock(m_mutex);
        if (m_queue_.empty()) {
            return std::nullopt;
        }
        return m_queue_.front();
    }

    /**
     * @brief Get the back element without removing it
     * @return Optional containing the back element or nothing if queue is empty
     */
    [[nodiscard]] auto back() const -> std::optional<T> {
        lock_guard lock(m_mutex);
        if (m_queue_.empty()) {
            return std::nullopt;
        }
        return m_queue_.back();
    }

    /**
     * @brief Emplace an element in the queue
     * @param args Arguments to construct the element
     * @throws std::bad_alloc if memory allocation fails
     */
    template <typename... Args>
        requires std::constructible_from<T, Args...>
    void emplace(Args&&... args) {
        try {
            {
                lock_guard lock(m_mutex);
                m_queue_.emplace(std::forward<Args>(args)...);
            }
            m_conditionVariable_.notify_one();
        } catch (const std::exception& e) {
            // Log error
        }
    }

    /**
     * @brief Wait for an element satisfying a predicate
     * @param predicate Function to check if an element satisfies a condition
     * @return Optional containing the element or nothing if queue is being
     * destroyed
     */
    template <std::predicate<const T&> Predicate>
    [[nodiscard]] auto waitFor(Predicate predicate) -> std::optional<T> {
        std::unique_lock<HybridMutex> lock(m_mutex);
        m_conditionVariable_.wait(lock, [this, &predicate] {
            return m_mustReturnNullptr_ ||
                   (!m_queue_.empty() && predicate(m_queue_.front()));
        });

        if (m_mustReturnNullptr_ || m_queue_.empty())
            return std::nullopt;

        T ret = std::move(m_queue_.front());
        m_queue_.pop();

        return ret;
    }

    /**
     * @brief Wait until the queue becomes empty
     */
    void waitUntilEmpty() noexcept {
        std::unique_lock<HybridMutex> lock(m_mutex);
        m_conditionVariable_.wait(
            lock, [this] { return m_mustReturnNullptr_ || m_queue_.empty(); });
    }

    /**
     * @brief Extract elements that satisfy a predicate
     * @param pred Predicate function
     * @return Vector of extracted elements
     */
    template <ExtractableWith<const T&> UnaryPredicate>
    [[nodiscard]] auto extractIf(UnaryPredicate pred) -> std::vector<T> {
        std::vector<T> result;
        {
            lock_guard lock(m_mutex);
            if (m_queue_.empty()) {
                return result;
            }

            const size_t queueSize = m_queue_.size();
            result.reserve(queueSize);  // Pre-allocate memory

            // Optimization: avoid unnecessary queue rebuilding, use dual-queue
            // swap method
            std::queue<T> remaining;

            while (!m_queue_.empty()) {
                T& item = m_queue_.front();
                if (pred(item)) {
                    result.push_back(std::move(item));
                } else {
                    remaining.push(std::move(item));
                }
                m_queue_.pop();
            }
            // Use swap to avoid copying, O(1) complexity
            std::swap(m_queue_, remaining);
        }
        return result;
    }

    /**
     * @brief Sort the elements in the queue
     * @param comp Comparison function
     */
    template <typename Compare>
        requires std::predicate<Compare, const T&, const T&>
    void sort(Compare comp) {
        lock_guard lock(m_mutex);
        if (m_queue_.empty()) {
            return;
        }

        std::vector<T> temp;
        temp.reserve(m_queue_.size());

        while (!m_queue_.empty()) {
            temp.push_back(std::move(m_queue_.front()));
            m_queue_.pop();
        }

        // Use parallel algorithm when available
        if (temp.size() > 1000) {
            std::sort(std::execution::par, temp.begin(), temp.end(), comp);
        } else {
            std::sort(temp.begin(), temp.end(), comp);
        }

        for (auto& elem : temp) {
            m_queue_.push(std::move(elem));
        }
    }

    /**
     * @brief Transform elements using a function and return a new queue
     * @param func Transformation function
     * @return Shared pointer to a queue of transformed elements
     */
    template <typename ResultType>
    [[nodiscard]] auto transform(std::function<ResultType(T)> func)
        -> std::shared_ptr<ThreadSafeQueue<ResultType>> {
        auto resultQueue = std::make_shared<ThreadSafeQueue<ResultType>>();

        // First get data, minimize lock holding time
        std::vector<T> originalItems;
        {
            lock_guard lock(m_mutex);
            if (m_queue_.empty()) {
                return resultQueue;
            }

            const size_t queueSize = m_queue_.size();
            originalItems.reserve(queueSize);

            // Use move semantics to reduce copying
            while (!m_queue_.empty()) {
                originalItems.push_back(std::move(m_queue_.front()));
                m_queue_.pop();
            }
        }

        // Process data outside the lock
        if (originalItems.size() > 1000) {
            std::vector<ResultType> transformed(originalItems.size());
            std::transform(std::execution::par, originalItems.begin(),
                           originalItems.end(), transformed.begin(), func);

            for (auto& item : transformed) {
                resultQueue->put(std::move(item));
            }
        } else {
            for (auto& item : originalItems) {
                resultQueue->put(func(std::move(item)));
            }
        }

        // Restore queue
        {
            lock_guard lock(m_mutex);
            for (auto& item : originalItems) {
                m_queue_.push(std::move(item));
            }
        }

        return resultQueue;
    }

    /**
     * @brief Group elements by a key
     * @param func Function to extract the key
     * @return Vector of queues, each containing elements with the same key
     */
    template <typename GroupKey>
        requires std::movable<GroupKey> && std::equality_comparable<GroupKey>
    [[nodiscard]] auto groupBy(std::function<GroupKey(const T&)> func)
        -> std::vector<std::shared_ptr<ThreadSafeQueue<T>>> {
        // Optimization: use reserve to pre-allocate hash table space, reduce
        // rehash operations
        std::unordered_map<GroupKey, std::shared_ptr<ThreadSafeQueue<T>>>
            resultMap;
        std::vector<T> originalItems;

        // Minimize lock holding time
        {
            lock_guard lock(m_mutex);
            if (m_queue_.empty()) {
                return {};
            }

            const size_t queueSize = m_queue_.size();
            originalItems.reserve(queueSize);

            // Use move semantics to reduce copying
            while (!m_queue_.empty()) {
                originalItems.push_back(std::move(m_queue_.front()));
                m_queue_.pop();
            }
        }

        // Process data outside the lock
        // Estimate map size, reduce rehash
        resultMap.reserve(std::min(originalItems.size(), size_t(100)));

        for (const auto& item : originalItems) {
            GroupKey key = func(item);
            if (!resultMap.contains(key)) {
                resultMap[key] = std::make_shared<ThreadSafeQueue<T>>();
            }
            resultMap[key]->put(
                item);  // Use constant reference to avoid copying
        }

        // Restore queue, prepare data outside the lock to reduce lock holding
        // time
        {
            lock_guard lock(m_mutex);
            for (auto& item : originalItems) {
                m_queue_.push(std::move(item));
            }
        }

        std::vector<std::shared_ptr<ThreadSafeQueue<T>>> resultQueues;
        resultQueues.reserve(resultMap.size());
        for (auto& [_, queue_ptr] : resultMap) {
            resultQueues.push_back(std::move(queue_ptr));  // Use move semantics
        }

        return resultQueues;
    }

    /**
     * @brief Convert queue contents to a vector
     * @return Vector containing copies of all elements
     */
    [[nodiscard]] auto toVector() const -> std::vector<T> {
        lock_guard lock(m_mutex);
        if (m_queue_.empty()) {
            return {};
        }

        const size_t queueSize = m_queue_.size();
        std::vector<T> result;
        result.reserve(queueSize);

        // Optimization: avoid creating temporary queue, use existing queue
        // directly
        std::queue<T> queueCopy = m_queue_;

        while (!queueCopy.empty()) {
            result.push_back(std::move(queueCopy.front()));
            queueCopy.pop();
        }

        return result;
    }

    /**
     * @brief Apply a function to each element
     * @param func Function to apply
     * @param parallel Whether to process in parallel
     */
    template <typename Func>
        requires std::invocable<Func, T&>
    void forEach(Func func, bool parallel = false) {
        std::vector<T> vec;
        {
            lock_guard lock(m_mutex);
            if (m_queue_.empty()) {
                return;
            }

            const size_t queueSize = m_queue_.size();
            vec.reserve(queueSize);

            // Use move semantics to reduce copying
            while (!m_queue_.empty()) {
                vec.push_back(std::move(m_queue_.front()));
                m_queue_.pop();
            }
        }

        // Process outside the lock to improve concurrency
        if (parallel && vec.size() > 1000) {
            std::for_each(std::execution::par, vec.begin(), vec.end(),
                          [&func](auto& item) { func(item); });
        } else {
            for (auto& item : vec) {
                func(item);
            }
        }

        // Restore queue
        {
            lock_guard lock(m_mutex);
            for (auto& item : vec) {
                m_queue_.push(std::move(item));
            }
        }
    }

    /**
     * @brief Try to take an element without waiting
     * @return Optional containing the element or nothing if queue is empty
     */
    [[nodiscard]] auto tryTake() noexcept -> std::optional<T> {
        lock_guard lock(m_mutex);
        if (m_queue_.empty()) {
            return std::nullopt;
        }
        T ret = std::move(m_queue_.front());
        m_queue_.pop();
        return ret;
    }

    /**
     * @brief Try to take an element with a timeout
     * @param timeout Maximum time to wait
     * @return Optional containing the element or nothing if timed out or queue
     * is being destroyed
     */
    template <typename Rep, typename Period>
    [[nodiscard]] auto takeFor(
        const std::chrono::duration<Rep, Period>& timeout) -> std::optional<T> {
        std::unique_lock<HybridMutex> lock(m_mutex);
        if (m_conditionVariable_.wait_for(lock, timeout, [this] {
                return !m_queue_.empty() || m_mustReturnNullptr_;
            })) {
            if (m_mustReturnNullptr_ || m_queue_.empty()) {
                return std::nullopt;
            }
            T ret = std::move(m_queue_.front());
            m_queue_.pop();
            return ret;
        }
        return std::nullopt;
    }

    /**
     * @brief Try to take an element until a time point
     * @param timeout_time Time point until which to wait
     * @return Optional containing the element or nothing if timed out or queue
     * is being destroyed
     */
    template <typename Clock, typename Duration>
    [[nodiscard]] auto takeUntil(
        const std::chrono::time_point<Clock, Duration>& timeout_time)
        -> std::optional<T> {
        std::unique_lock<HybridMutex> lock(m_mutex);
        if (m_conditionVariable_.wait_until(lock, timeout_time, [this] {
                return !m_queue_.empty() || m_mustReturnNullptr_;
            })) {
            if (m_mustReturnNullptr_ || m_queue_.empty()) {
                return std::nullopt;
            }
            T ret = std::move(m_queue_.front());
            m_queue_.pop();
            return ret;
        }
        return std::nullopt;
    }

    /**
     * @brief Process batch of items in parallel
     * @param batchSize Size of each batch
     * @param processor Function to process each batch
     * @return Number of processed batches
     */
    template <typename Processor>
        requires std::invocable<Processor, std::span<T>>
    size_t processBatches(size_t batchSize, Processor processor) {
        if (batchSize == 0) {
            throw std::invalid_argument("Batch size must be positive");
        }

        std::vector<T> items;
        {
            lock_guard lock(m_mutex);
            if (m_queue_.empty()) {
                return 0;
            }

            items.reserve(m_queue_.size());
            while (!m_queue_.empty()) {
                items.push_back(std::move(m_queue_.front()));
                m_queue_.pop();
            }
        }

        size_t numBatches = (items.size() + batchSize - 1) / batchSize;
        std::vector<std::future<void>> futures;
        futures.reserve(numBatches);

        // Process batches in parallel
        for (size_t i = 0; i < items.size(); i += batchSize) {
            size_t end = std::min(i + batchSize, items.size());
            futures.push_back(
                std::async(std::launch::async, [&processor, &items, i, end]() {
                    std::span<T> batch(&items[i], end - i);
                    processor(batch);
                }));
        }

        // Wait for all batches to complete
        for (auto& future : futures) {
            future.wait();
        }

        // Put processed items back
        {
            lock_guard lock(m_mutex);
            for (auto& item : items) {
                m_queue_.push(std::move(item));
            }
        }

        return numBatches;
    }

    /**
     * @brief Apply a filter to the queue elements
     * @param predicate Predicate determining which elements to keep
     */
    template <std::predicate<const T&> Predicate>
    void filter(Predicate predicate) {
        lock_guard lock(m_mutex);
        if (m_queue_.empty()) {
            return;
        }

        std::queue<T> filtered;
        while (!m_queue_.empty()) {
            T item = std::move(m_queue_.front());
            m_queue_.pop();

            if (predicate(item)) {
                filtered.push(std::move(item));
            }
        }

        std::swap(m_queue_, filtered);
    }

    /**
     * @brief Filter elements and return a new queue with matching elements
     * @param predicate Predicate determining which elements to include
     * @return Shared pointer to a new queue containing filtered elements
     */
    template <std::predicate<const T&> Predicate>
    [[nodiscard]] auto filterOut(Predicate predicate)
        -> std::shared_ptr<ThreadSafeQueue<T>> {
        auto resultQueue = std::make_shared<ThreadSafeQueue<T>>();

        std::vector<T> originalItems;

        {
            lock_guard lock(m_mutex);
            if (m_queue_.empty()) {
                return resultQueue;
            }

            // Extract all items to process them outside the lock
            originalItems.reserve(m_queue_.size());

            while (!m_queue_.empty()) {
                originalItems.push_back(std::move(m_queue_.front()));
                m_queue_.pop();
            }
        }

        // Process items and separate them based on predicate
        std::vector<T> remainingItems;
        remainingItems.reserve(originalItems.size());

        for (auto& item : originalItems) {
            if (predicate(item)) {
                resultQueue->put(T(item));  // Copy item to result queue
            }
            remainingItems.push_back(
                std::move(item));  // Move back to original queue
        }

        // Restore remaining items to the queue
        {
            lock_guard lock(m_mutex);
            for (auto& item : remainingItems) {
                m_queue_.push(std::move(item));
            }
        }

        return resultQueue;
    }

private:
    std::queue<T> m_queue_;
    mutable HybridMutex m_mutex;  // High-performance hybrid mutex
    std::condition_variable_any m_conditionVariable_;
    std::atomic<bool> m_mustReturnNullptr_{false};

    // Add padding to prevent false sharing
    alignas(std::hardware_destructive_interference_size) char m_padding[1];
};

/**
 * @brief Memory-pooled thread-safe queue implementation
 * @tparam T Type of elements stored in the queue
 * @tparam MemoryPoolSize Size of memory pool, default is 1MB
 */
template <Movable T, size_t MemoryPoolSize = 1024 * 1024>
class PooledThreadSafeQueue {
public:
    PooledThreadSafeQueue()
        : m_memoryPool_(buffer_, MemoryPoolSize), m_resource_(&m_memoryPool_) {}

    PooledThreadSafeQueue(const PooledThreadSafeQueue&) = delete;
    PooledThreadSafeQueue& operator=(const PooledThreadSafeQueue&) = delete;
    PooledThreadSafeQueue(PooledThreadSafeQueue&&) noexcept = default;
    PooledThreadSafeQueue& operator=(PooledThreadSafeQueue&&) noexcept =
        default;

    ~PooledThreadSafeQueue() noexcept {
        try {
            destroy();
        } catch (...) {
            // Ensure no exceptions escape destructor
        }
    }

    /**
     * @brief Add an element to the queue
     * @param element Element to be added
     */
    void put(T element) noexcept(std::is_nothrow_move_constructible_v<T>) {
        try {
            {
                lock_guard lock(m_mutex);
                m_queue_.push(std::move(element));
            }
            m_conditionVariable_.notify_one();
        } catch (const std::exception&) {
            // Error handling
        }
    }

    /**
     * @brief Take an element from the queue
     * @return Optional containing the element or nothing if queue is being
     * destroyed
     */
    [[nodiscard]] auto take() -> std::optional<T> {
        std::unique_lock<HybridMutex> lock(m_mutex);
        while (!m_mustReturnNullptr_ && m_queue_.empty()) {
            m_conditionVariable_.wait(lock);
        }

        if (m_mustReturnNullptr_ || m_queue_.empty()) {
            return std::nullopt;
        }

        std::optional<T> ret{std::move(m_queue_.front())};
        m_queue_.pop();
        return ret;
    }

    /**
     * @brief Destroy the queue and return remaining elements
     * @return Queue containing all remaining elements
     */
    [[nodiscard]] auto destroy() noexcept -> std::queue<T> {
        {
            lock_guard lock(m_mutex);
            m_mustReturnNullptr_ = true;
        }
        m_conditionVariable_.notify_all();

        std::queue<T> result(&m_resource_);
        {
            lock_guard lock(m_mutex);
            std::swap(result, m_queue_);
        }
        return result;
    }

    /**
     * @brief Get the size of the queue
     * @return Current queue size
     */
    [[nodiscard]] auto size() const noexcept -> size_t {
        lock_guard lock(m_mutex);
        return m_queue_.size();
    }

    /**
     * @brief Check if the queue is empty
     * @return True if queue is empty, false otherwise
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        lock_guard lock(m_mutex);
        return m_queue_.empty();
    }

    /**
     * @brief Clear all elements from the queue
     */
    void clear() noexcept {
        lock_guard lock(m_mutex);
        // Create a new empty queue using PMR memory resource
        std::queue<T> empty(&m_resource_);
        std::swap(m_queue_, empty);
    }

    /**
     * @brief Get the front element without removing it
     * @return Optional containing the front element or nothing if queue is
     * empty
     */
    [[nodiscard]] auto front() const -> std::optional<T> {
        lock_guard lock(m_mutex);
        if (m_queue_.empty()) {
            return std::nullopt;
        }
        return m_queue_.front();
    }

private:
    // Use PMR memory pool and queue
    alignas(std::hardware_destructive_interference_size) char buffer_
        [MemoryPoolSize];
    std::pmr::monotonic_buffer_resource m_memoryPool_;
    std::pmr::polymorphic_allocator<T> m_resource_;
    std::queue<T> m_queue_{&m_resource_};

    mutable HybridMutex m_mutex;
    std::condition_variable_any m_conditionVariable_;
    std::atomic<bool> m_mustReturnNullptr_{false};
};

}  // namespace atom::async

#ifdef ATOM_USE_LOCKFREE_QUEUE

namespace atom::async {
/**
 * @brief Lock-free queue implementation using boost::lockfree
 * @tparam T Type of elements stored in the queue
 */
template <Movable T>
class LockFreeQueue {
public:
    /**
     * @brief Construct a new Lock Free Queue
     * @param capacity Initial capacity of the queue
     */
    explicit LockFreeQueue(size_t capacity = 128) : m_queue_(capacity) {}

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
    LockFreeQueue(LockFreeQueue&&) = delete;
    LockFreeQueue& operator=(LockFreeQueue&&) = delete;

    /**
     * @brief Add an element to the queue
     * @param element Element to be added
     * @return True if successful, false if queue is full
     */
    bool put(const T& element) noexcept { return m_queue_.push(element); }

    /**
     * @brief Add an element to the queue
     * @param element Element to be added
     * @return True if successful, false if queue is full
     */
    bool put(T&& element) noexcept { return m_queue_.push(std::move(element)); }

    /**
     * @brief Take an element from the queue
     * @return Optional containing the element or nothing if queue is empty
     */
    [[nodiscard]] auto take() -> std::optional<T> {
        T item;
        if (m_queue_.pop(item)) {
            return item;
        }
        return std::nullopt;
    }

    /**
     * @brief Check if the queue is empty
     * @return True if queue is empty
     */
    [[nodiscard]] bool empty() const noexcept { return m_queue_.empty(); }

    /**
     * @brief Check if the queue is full
     * @return True if queue is full
     */
    [[nodiscard]] bool full() const noexcept { return m_queue_.full(); }

    /**
     * @brief Resize the queue
     * @param capacity New capacity
     * @note This operation is not safe to call concurrently with other
     * operations
     */
    void resize(size_t capacity) { m_queue_.reserve(capacity); }

    /**
     * @brief Get the capacity of the queue
     * @return Current maximum capacity of the queue
     */
    [[nodiscard]] size_t capacity() const noexcept {
        return m_queue_.capacity();
    }

    /**
     * @brief Try to take an element without waiting
     * @return Optional containing the element or nothing if queue is empty
     */
    [[nodiscard]] auto tryTake() noexcept -> std::optional<T> {
        return take();  // Same as take() for lockfree queue
    }

    /**
     * @brief Process batch of items
     * @param processor Function to process each item
     * @param maxItems Maximum number of items to process
     * @return Number of processed items
     */
    template <typename Processor>
        requires std::invocable<Processor, T&>
    size_t consume(Processor processor, size_t maxItems = SIZE_MAX) {
        return m_queue_.consume_all([&processor](T& item) { processor(item); });
    }

private:
    boost::lockfree::queue<T> m_queue_;
};

/**
 * @brief Single-producer, single-consumer lock-free queue
 * @tparam T Type of elements stored in the queue
 */
template <Movable T>
class SPSCQueue {
public:
    /**
     * @brief Construct a new SPSC Queue
     * @param capacity Initial capacity of the queue
     */
    explicit SPSCQueue(size_t capacity = 128) : m_queue_(capacity) {}

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    /**
     * @brief Add an element to the queue
     * @param element Element to be added
     * @return True if successful, false if queue is full
     */
    bool put(const T& element) noexcept { return m_queue_.push(element); }

    /**
     * @brief Take an element from the queue
     * @return Optional containing the element or nothing if queue is empty
     */
    [[nodiscard]] auto take() -> std::optional<T> {
        T item;
        if (m_queue_.pop(item)) {
            return item;
        }
        return std::nullopt;
    }

    /**
     * @brief Check if the queue is empty
     * @return True if queue is empty
     */
    [[nodiscard]] bool empty() const noexcept { return m_queue_.empty(); }

    /**
     * @brief Check if the queue is full
     * @return True if queue is full
     */
    [[nodiscard]] bool full() const noexcept { return m_queue_.full(); }

    /**
     * @brief Get the capacity of the queue
     * @return Current maximum capacity of the queue
     */
    [[nodiscard]] size_t capacity() const noexcept {
        return m_queue_.capacity();
    }

private:
    boost::lockfree::spsc_queue<T> m_queue_;
};

}  // namespace atom::async

#endif  // ATOM_USE_LOCKFREE_QUEUE

#ifdef ATOM_USE_LOCKFREE_QUEUE
/**
 * @brief Queue type selection based on characteristics and requirements
 */
template <Movable T>
class QueueSelector {
public:
    /**
     * @brief Select appropriate queue type based on parameters
     * @param capacity Initial capacity
     * @param singleProducerConsumer Whether to use SPSC queue
     * @return Appropriate queue implementation
     */
    static auto select(size_t capacity = 128,
                       bool singleProducerConsumer = false) {
        if (singleProducerConsumer) {
            return std::make_unique<SPSCQueue<T>>(capacity);
        } else {
            return std::make_unique<LockFreeQueue<T>>(capacity);
        }
    }

    /**
     * @brief Create a thread-safe queue (blocking implementation)
     * @return Thread-safe queue instance
     */
    static auto createThreadSafe() {
        return std::make_unique<ThreadSafeQueue<T>>();
    }

    /**
     * @brief Create a lock-free queue
     * @param capacity Initial capacity
     * @return Lock-free queue instance
     */
    static auto createLockFree(size_t capacity = 128) {
        return std::make_unique<LockFreeQueue<T>>(capacity);
    }

    /**
     * @brief Create a single-producer, single-consumer queue
     * @param capacity Initial capacity
     * @return SPSC queue instance
     */
    static auto createSPSC(size_t capacity = 128) {
        return std::make_unique<SPSCQueue<T>>(capacity);
    }
};
#endif  // ATOM_USE_LOCKFREE_QUEUE

// Add performance benchmark suite
#ifdef ATOM_QUEUE_BENCHMARK
namespace atom::async {

/**
 * @brief Queue performance benchmark utility class
 * @tparam Q Queue type
 * @tparam T Element type
 */
template <template <typename> class Q, typename T>
class QueueBenchmark {
public:
    // Test put/take performance with elements of different sizes
    static void benchmarkPutTake(size_t numOperations,
                                 size_t elementSize = sizeof(T)) {
        Q<std::vector<char>> queue;

        // Fill element to reach specified size
        std::vector<char> element(elementSize, 'X');

        auto start = std::chrono::high_resolution_clock::now();

        // Put operations
        for (size_t i = 0; i < numOperations; ++i) {
            queue.put(element);
        }

        // Take operations
        for (size_t i = 0; i < numOperations; ++i) {
            auto result = queue.take();
            if (!result)
                break;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Queue type: " << typeid(Q<T>).name() << "\n";
        std::cout << "Element size: " << elementSize << " bytes\n";
        std::cout << "Operations: " << numOperations << " puts + "
                  << numOperations << " takes\n";
        std::cout << "Total time: " << duration.count() << " µs\n";
        std::cout << "Average time per operation: "
                  << duration.count() / (numOperations * 2.0) << " µs\n";
        std::cout << "----------------------------------------\n";
    }

    // Test multi-producer, multi-consumer performance
    static void benchmarkMultiThreaded(size_t numProducers, size_t numConsumers,
                                       size_t itemsPerProducer) {
        Q<size_t> queue;
        std::atomic<size_t> producedCount = 0;
        std::atomic<size_t> consumedCount = 0;

        auto start = std::chrono::high_resolution_clock::now();

        // Create producer threads
        std::vector<std::thread> producers;
        for (size_t p = 0; p < numProducers; ++p) {
            producers.emplace_back(
                [&queue, &producedCount, itemsPerProducer, p]() {
                    for (size_t i = 0; i < itemsPerProducer; ++i) {
                        queue.put(p * itemsPerProducer + i);
                        producedCount.fetch_add(1, std::memory_order_relaxed);
                    }
                });
        }

        // Create consumer threads
        std::vector<std::thread> consumers;
        const size_t totalItems = numProducers * itemsPerProducer;
        for (size_t c = 0; c < numConsumers; ++c) {
            consumers.emplace_back([&queue, &consumedCount, totalItems]() {
                while (consumedCount.load(std::memory_order_relaxed) <
                       totalItems) {
                    auto item = queue.take();
                    if (item) {
                        consumedCount.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        // Wait for all threads to complete
        for (auto& p : producers)
            p.join();
        for (auto& c : consumers)
            c.join();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Queue type: " << typeid(Q<T>).name() << "\n";
        std::cout << "Threads: " << numProducers << " producers, "
                  << numConsumers << " consumers\n";
        std::cout << "Total items: " << totalItems << "\n";
        std::cout << "Total time: " << duration.count() << " µs\n";
        std::cout << "Throughput: "
                  << (totalItems * 1000000.0) / duration.count()
                  << " ops/sec\n";
        std::cout << "----------------------------------------\n";
    }
};

}  // namespace atom::async
#endif  // ATOM_QUEUE_BENCHMARK

#endif  // ATOM_ASYNC_QUEUE_HPP
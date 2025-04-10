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
#include <mutex>
#include <optional>
#include <queue>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace atom::async {

// Concepts for improved compile-time type checking
template <typename T>
concept Movable = std::move_constructible<T> && std::assignable_from<T&, T>;

template <typename T, typename U>
concept ExtractableWith = requires(T t, U u) {
    { u(t) } -> std::convertible_to<bool>;
};

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
                std::lock_guard lock(m_mutex_);
                m_queue_.push(std::move(element));
            }
            m_conditionVariable_.notify_one();
        } catch (const std::exception& e) {
            // Log error - in a real implementation, use proper logging
        }
    }

    /**
     * @brief Take an element from the queue
     * @return Optional containing the element or nothing if queue is being
     * destroyed
     */
    [[nodiscard]] auto take() -> std::optional<T> {
        std::unique_lock lock(m_mutex_);
        m_conditionVariable_.wait(
            lock, [this] { return m_mustReturnNullptr_ || !m_queue_.empty(); });

        if (m_mustReturnNullptr_ || m_queue_.empty()) {
            return std::nullopt;
        }

        T ret = std::move(m_queue_.front());
        m_queue_.pop();

        return ret;
    }

    /**
     * @brief Destroy the queue and return remaining elements
     * @return Queue containing all remaining elements
     */
    [[nodiscard]] auto destroy() noexcept -> std::queue<T> {
        {
            std::lock_guard lock(m_mutex_);
            m_mustReturnNullptr_ = true;
        }
        m_conditionVariable_.notify_all();

        std::queue<T> result;
        {
            std::lock_guard lock(m_mutex_);
            std::swap(result, m_queue_);
        }
        return result;
    }

    /**
     * @brief Get the size of the queue
     * @return Current size of the queue
     */
    [[nodiscard]] auto size() const noexcept -> size_t {
        std::lock_guard lock(m_mutex_);
        return m_queue_.size();
    }

    /**
     * @brief Check if the queue is empty
     * @return True if queue is empty, false otherwise
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        std::lock_guard lock(m_mutex_);
        return m_queue_.empty();
    }

    /**
     * @brief Clear all elements from the queue
     */
    void clear() noexcept {
        std::lock_guard lock(m_mutex_);
        std::queue<T> empty;
        std::swap(m_queue_, empty);
    }

    /**
     * @brief Get the front element without removing it
     * @return Optional containing the front element or nothing if queue is
     * empty
     */
    [[nodiscard]] auto front() const -> std::optional<T> {
        std::lock_guard lock(m_mutex_);
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
        std::lock_guard lock(m_mutex_);
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
                std::lock_guard lock(m_mutex_);
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
        std::unique_lock lock(m_mutex_);
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
        std::unique_lock lock(m_mutex_);
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
            std::lock_guard lock(m_mutex_);
            if (m_queue_.empty()) {
                return result;
            }

            result.reserve(m_queue_.size());  // Pre-allocate memory
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
        std::lock_guard lock(m_mutex_);
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
        {
            std::lock_guard lock(m_mutex_);
            if (m_queue_.empty()) {
                return resultQueue;
            }

            std::vector<T> original;
            original.reserve(m_queue_.size());

            while (!m_queue_.empty()) {
                original.push_back(std::move(m_queue_.front()));
                m_queue_.pop();
            }

            // Use parallel transform for large collections
            if (original.size() > 1000) {
                std::vector<ResultType> transformed(original.size());
                std::transform(std::execution::par, original.begin(),
                               original.end(), transformed.begin(), func);

                for (auto& item : transformed) {
                    resultQueue->put(std::move(item));
                }
            } else {
                for (auto& item : original) {
                    resultQueue->put(func(std::move(item)));
                }
            }

            // Restore original items to the queue
            for (auto& item : original) {
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
        std::unordered_map<GroupKey, std::shared_ptr<ThreadSafeQueue<T>>>
            resultMap;
        std::vector<T> originalItems;

        {
            std::lock_guard lock(m_mutex_);
            if (m_queue_.empty()) {
                return {};
            }

            originalItems.reserve(m_queue_.size());

            while (!m_queue_.empty()) {
                originalItems.push_back(std::move(m_queue_.front()));
                m_queue_.pop();
            }
        }

        // Process items
        for (auto& item : originalItems) {
            GroupKey key = func(item);
            if (!resultMap.contains(key)) {
                resultMap[key] = std::make_shared<ThreadSafeQueue<T>>();
            }
            resultMap[key]->put(T(item));  // Make a copy for the result map

            // Put back into the original queue
            m_queue_.push(std::move(item));
        }

        std::vector<std::shared_ptr<ThreadSafeQueue<T>>> resultQueues;
        resultQueues.reserve(resultMap.size());
        for (auto& [_, queue_ptr] : resultMap) {
            resultQueues.push_back(queue_ptr);
        }

        return resultQueues;
    }

    /**
     * @brief Convert queue contents to a vector
     * @return Vector containing copies of all elements
     */
    [[nodiscard]] auto toVector() const -> std::vector<T> {
        std::lock_guard lock(m_mutex_);
        if (m_queue_.empty()) {
            return {};
        }

        std::vector<T> result;
        result.reserve(m_queue_.size());

        // We can't directly convert from queue to vector
        // Need to make a copy of the queue first
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
            std::lock_guard lock(m_mutex_);
            if (m_queue_.empty()) {
                return;
            }

            vec.reserve(m_queue_.size());
            std::queue<T> tempQueue;

            // Extract all items
            while (!m_queue_.empty()) {
                vec.push_back(std::move(m_queue_.front()));
                m_queue_.pop();
            }
        }

        // Process items
        if (parallel && vec.size() > 1000) {
            // Use parallel execution policy
            std::for_each(std::execution::par, vec.begin(), vec.end(),
                          [&func](auto& item) { func(item); });
        } else {
            // Sequential processing
            for (auto& item : vec) {
                func(item);
            }
        }

        // Put items back into the queue
        {
            std::lock_guard lock(m_mutex_);
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
        std::lock_guard lock(m_mutex_);
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
        std::unique_lock lock(m_mutex_);
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
        std::unique_lock lock(m_mutex_);
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
            std::lock_guard lock(m_mutex_);
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
            std::lock_guard lock(m_mutex_);
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
        std::lock_guard lock(m_mutex_);
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

private:
    std::queue<T> m_queue_;
    mutable std::mutex m_mutex_;
    std::condition_variable m_conditionVariable_;
    std::atomic<bool> m_mustReturnNullptr_{false};
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_QUEUE_HPP

// Add after the ThreadSafeQueue class definition

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
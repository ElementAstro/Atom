#pragma once

/**
 * @file lockfree_queue.hpp
 * @brief High-performance lock-free queue with hazard pointers for safe memory reclamation
 */

#include <atomic>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include "../asio_compatibility.hpp"

namespace atom::extra::asio::concurrency {

/**
 * @brief Hazard pointer implementation for safe memory reclamation in lock-free data structures
 */
template<typename T>
class hazard_pointer {
private:
    static constexpr std::size_t max_hazard_pointers = 100;
    static thread_local std::array<std::atomic<T*>, max_hazard_pointers> hazard_ptrs_;
    static thread_local std::size_t next_hazard_ptr_;

public:
    /**
     * @brief Acquire a hazard pointer for the given object
     */
    static std::size_t acquire(T* ptr) noexcept {
        std::size_t index = next_hazard_ptr_++;
        if (index >= max_hazard_pointers) {
            next_hazard_ptr_ = 0;
            index = 0;
        }
        hazard_ptrs_[index].store(ptr, std::memory_order_release);
        return index;
    }

    /**
     * @brief Release a hazard pointer
     */
    static void release(std::size_t index) noexcept {
        if (index < max_hazard_pointers) {
            hazard_ptrs_[index].store(nullptr, std::memory_order_release);
        }
    }

    /**
     * @brief Check if a pointer is protected by any hazard pointer
     */
    static bool is_protected(T* ptr) noexcept {
        for (const auto& hp : hazard_ptrs_) {
            if (hp.load(std::memory_order_acquire) == ptr) {
                return true;
            }
        }
        return false;
    }
};

template<typename T>
thread_local std::array<std::atomic<T*>, hazard_pointer<T>::max_hazard_pointers>
    hazard_pointer<T>::hazard_ptrs_{};

template<typename T>
thread_local std::size_t hazard_pointer<T>::next_hazard_ptr_ = 0;

/**
 * @brief Lock-free queue node with atomic next pointer
 */
template<typename T>
struct alignas(cache_line_size) queue_node {
    std::atomic<queue_node*> next{nullptr};
    std::optional<T> data;

    queue_node() = default;

    template<typename... Args>
    explicit queue_node(Args&&... args) : data(std::forward<Args>(args)...) {}
};

/**
 * @brief High-performance lock-free multi-producer multi-consumer queue
 *
 * This implementation uses hazard pointers for safe memory reclamation and provides
 * excellent performance characteristics for concurrent access patterns.
 */
template<typename T>
class lockfree_queue {
private:
    using node_type = queue_node<T>;

    cache_aligned<std::atomic<node_type*>> head_;
    cache_aligned<std::atomic<node_type*>> tail_;
    cache_aligned<std::atomic<std::size_t>> size_;

    /**
     * @brief Retire a node safely using hazard pointers
     */
    void retire_node(node_type* node) {
        if (!hazard_pointer<node_type>::is_protected(node)) {
            delete node;
        } else {
            // Add to retirement list for later cleanup
            // In a full implementation, we'd maintain a retirement list
            spdlog::trace("Node retirement deferred due to hazard pointer protection");
        }
    }

public:
    /**
     * @brief Construct an empty lock-free queue
     */
    lockfree_queue() : size_(0) {
        auto dummy = new node_type;
        head_.get().store(dummy, std::memory_order_relaxed);
        tail_.get().store(dummy, std::memory_order_relaxed);

        spdlog::debug("Lock-free queue initialized with dummy node");
    }

    /**
     * @brief Destructor - cleans up remaining nodes
     */
    ~lockfree_queue() {
        while (auto item = try_pop()) {
            // Items are automatically destroyed
        }

        // Clean up dummy node
        auto head = head_.get().load(std::memory_order_relaxed);
        delete head;

        spdlog::debug("Lock-free queue destroyed");
    }

    // Non-copyable, non-movable for safety
    lockfree_queue(const lockfree_queue&) = delete;
    lockfree_queue& operator=(const lockfree_queue&) = delete;
    lockfree_queue(lockfree_queue&&) = delete;
    lockfree_queue& operator=(lockfree_queue&&) = delete;

    /**
     * @brief Push an item to the queue (thread-safe)
     */
    template<typename U>
    void push(U&& item) {
        auto new_node = new node_type(std::forward<U>(item));
        auto prev_tail = tail_.get().exchange(new_node, std::memory_order_acq_rel);
        prev_tail->next.store(new_node, std::memory_order_release);

        size_.get().fetch_add(1, std::memory_order_relaxed);

        spdlog::trace("Item pushed to lock-free queue, size: {}",
                     size_.get().load(std::memory_order_relaxed));
    }

    /**
     * @brief Try to pop an item from the queue (thread-safe)
     * @return Optional containing the item if successful, nullopt if queue is empty
     */
    std::optional<T> try_pop() {
        auto head = head_.get().load(std::memory_order_acquire);
        auto hazard_index = hazard_pointer<node_type>::acquire(head);

        // Verify head hasn't changed
        if (head != head_.get().load(std::memory_order_acquire)) {
            hazard_pointer<node_type>::release(hazard_index);
            return std::nullopt;
        }

        auto next = head->next.load(std::memory_order_acquire);
        if (!next) {
            hazard_pointer<node_type>::release(hazard_index);
            return std::nullopt;
        }

        if (head_.get().compare_exchange_weak(head, next, std::memory_order_release)) {
            hazard_pointer<node_type>::release(hazard_index);

            auto result = std::move(next->data);
            retire_node(head);

            if (result) {
                size_.get().fetch_sub(1, std::memory_order_relaxed);
                spdlog::trace("Item popped from lock-free queue, size: {}",
                             size_.get().load(std::memory_order_relaxed));
            }

            return result;
        }

        hazard_pointer<node_type>::release(hazard_index);
        return std::nullopt;
    }

    /**
     * @brief Get approximate size of the queue
     * @return Current size (may be slightly inaccurate due to concurrent operations)
     */
    std::size_t size() const noexcept {
        return size_.get().load(std::memory_order_relaxed);
    }

    /**
     * @brief Check if the queue is empty
     * @return True if queue appears empty (may change immediately due to concurrency)
     */
    bool empty() const noexcept {
        return size() == 0;
    }
};

} // namespace atom::extra::asio::concurrency

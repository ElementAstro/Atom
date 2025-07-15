/*
 * atom/containers/lockfree.hpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-11

Description: Boost Lock-Free Data Structures

**************************************************/

#pragma once

#include "../macro.hpp"

// Enable only if ATOM_HAS_BOOST_LOCKFREE is defined and Boost lock-free library
// is available
#if defined(ATOM_HAS_BOOST_LOCKFREE)

#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/stack.hpp>

namespace atom {
namespace containers {
namespace lockfree {

/**
 * @brief Multi-producer multi-consumer lock-free queue
 *
 * This queue allows multiple threads to enqueue and dequeue concurrently
 * without mutex locks. Suitable for high-performance concurrent systems and
 * parallel computing.
 *
 * @tparam T Element type
 * @tparam Capacity Queue capacity
 */
template <typename T, size_t Capacity = 1024>
class queue {
private:
    boost::lockfree::queue<T, boost::lockfree::capacity<Capacity>> impl_;

public:
    queue() : impl_() {}

    /**
     * @brief Push element to queue
     *
     * @param item Element to enqueue
     * @return bool Returns true if successful, false if queue is full
     */
    bool push(const T& item) { return impl_.push(item); }

    /**
     * @brief Pop element from queue
     *
     * @param item Reference to receive popped element
     * @return bool Returns true if successful, false if queue is empty
     */
    bool pop(T& item) { return impl_.pop(item); }

    /**
     * @brief Check if queue is empty
     *
     * Note: In multithreaded environments, this operation result may
     * immediately become outdated
     *
     * @return bool Returns true if queue is empty
     */
    bool empty() const { return impl_.empty(); }
};

/**
 * @brief Single-producer single-consumer lock-free queue
 *
 * This highly optimized queue is suitable for scenarios with only one thread
 * producing data and one thread consuming data. Has lower overhead than
 * multi-producer multi-consumer version.
 *
 * @tparam T Element type
 * @tparam Capacity Queue capacity
 */
template <typename T, size_t Capacity = 1024>
class spsc_queue {
private:
    boost::lockfree::spsc_queue<T, boost::lockfree::capacity<Capacity>> impl_;

public:
    spsc_queue() : impl_() {}

    /**
     * @brief Push element to queue
     *
     * @param item Element to enqueue
     * @return bool Returns true if successful, false if queue is full
     */
    bool push(const T& item) { return impl_.push(item); }

    /**
     * @brief Pop element from queue
     *
     * @param item Reference to receive popped element
     * @return bool Returns true if successful, false if queue is empty
     */
    bool pop(T& item) { return impl_.pop(item); }

    /**
     * @brief Check if queue is empty
     *
     * @return bool Returns true if queue is empty
     */
    bool empty() const { return impl_.empty(); }
};

/**
 * @brief Lock-free stack
 *
 * Thread-safe LIFO data structure that allows multiple threads to push and pop
 * elements concurrently without mutex locks.
 *
 * @tparam T Element type
 * @tparam Capacity Stack capacity
 */
template <typename T, size_t Capacity = 1024>
class stack {
private:
    boost::lockfree::stack<T, boost::lockfree::capacity<Capacity>> impl_;

public:
    stack() : impl_() {}

    /**
     * @brief Push element to stack
     *
     * @param item Element to push
     * @return bool Returns true if successful, false if stack is full
     */
    bool push(const T& item) { return impl_.push(item); }

    /**
     * @brief Pop element from stack
     *
     * @param item Reference to receive popped element
     * @return bool Returns true if successful, false if stack is empty
     */
    bool pop(T& item) { return impl_.pop(item); }

    /**
     * @brief Check if stack is empty
     *
     * Note: In multithreaded environments, this operation result may
     * immediately become outdated
     *
     * @return bool Returns true if stack is empty
     */
    bool empty() const { return impl_.empty(); }
};

}  // namespace lockfree
}  // namespace containers
}  // namespace atom

#endif  // defined(ATOM_HAS_BOOST_LOCKFREE)

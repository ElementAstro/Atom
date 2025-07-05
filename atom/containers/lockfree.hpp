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

// 只有在定义了ATOM_USE_BOOST_LOCKFREE宏且Boost锁无关库可用时才启用
#if defined(ATOM_HAS_BOOST_LOCKFREE)

#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/stack.hpp>

namespace atom {
namespace containers {
namespace lockfree {

/**
 * @brief 多生产者多消费者无锁队列
 *
 * 这个队列允许多个线程并发地入队和出队，无需互斥锁。
 * 适用于高性能并发系统和并行计算。
 *
 * @tparam T 元素类型
 * @tparam Capacity 队列容量
 */
template <typename T, size_t Capacity = 1024>
class queue {
private:
    boost::lockfree::queue<T, boost::lockfree::capacity<Capacity>> impl_;

public:
    queue() : impl_() {}

    /**
     * @brief 将元素推入队列
     *
     * @param item 要入队的元素
     * @return bool 如果成功返回true，如果队列已满则返回false
     */
    bool push(const T& item) { return impl_.push(item); }

    /**
     * @brief 从队列弹出元素
     *
     * @param item 接收弹出元素的引用
     * @return bool 如果成功返回true，如果队列为空则返回false
     */
    bool pop(T& item) { return impl_.pop(item); }

    /**
     * @brief 检查队列是否为空
     *
     * 注意：在多线程环境中，此操作结果可能立即过期
     *
     * @return bool 如果队列为空返回true
     */
    bool empty() const { return impl_.empty(); }
};

/**
 * @brief 单生产者单消费者无锁队列
 *
 * 这个高度优化的队列适用于只有一个线程生产数据和一个线程消费数据的场景。
 * 比多生产者多消费者版本有更低的开销。
 *
 * @tparam T 元素类型
 * @tparam Capacity 队列容量
 */
template <typename T, size_t Capacity = 1024>
class spsc_queue {
private:
    boost::lockfree::spsc_queue<T, boost::lockfree::capacity<Capacity>> impl_;

public:
    spsc_queue() : impl_() {}

    /**
     * @brief 将元素推入队列
     *
     * @param item 要入队的元素
     * @return bool 如果成功返回true，如果队列已满则返回false
     */
    bool push(const T& item) { return impl_.push(item); }

    /**
     * @brief 从队列弹出元素
     *
     * @param item 接收弹出元素的引用
     * @return bool 如果成功返回true，如果队列为空则返回false
     */
    bool pop(T& item) { return impl_.pop(item); }

    /**
     * @brief 检查队列是否为空
     *
     * @return bool 如果队列为空返回true
     */
    bool empty() const { return impl_.empty(); }
};

/**
 * @brief 无锁栈
 *
 * 线程安全的LIFO数据结构，允许多个线程并发地压入和弹出元素，无需互斥锁。
 *
 * @tparam T 元素类型
 * @tparam Capacity 栈容量
 */
template <typename T, size_t Capacity = 1024>
class stack {
private:
    boost::lockfree::stack<T, boost::lockfree::capacity<Capacity>> impl_;

public:
    stack() : impl_() {}

    /**
     * @brief 将元素压入栈
     *
     * @param item 要压入的元素
     * @return bool 如果成功返回true，如果栈已满则返回false
     */
    bool push(const T& item) { return impl_.push(item); }

    /**
     * @brief 从栈弹出元素
     *
     * @param item 接收弹出元素的引用
     * @return bool 如果成功返回true，如果栈为空则返回false
     */
    bool pop(T& item) { return impl_.pop(item); }

    /**
     * @brief 检查栈是否为空
     *
     * 注意：在多线程环境中，此操作结果可能立即过期
     *
     * @return bool 如果栈为空返回true
     */
    bool empty() const { return impl_.empty(); }
};

}  // namespace lockfree
}  // namespace containers
}  // namespace atom

#endif  // defined(ATOM_HAS_BOOST_LOCKFREE)
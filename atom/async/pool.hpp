/*
 * pool.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-13

Description: A very simple thread pool for preload

**************************************************/

#ifndef ATOM_ASYNC_POOL_HPP
#define ATOM_ASYNC_POOL_HPP

#include <algorithm>
#include <atomic>
#include <concepts>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <limits>
#include <mutex>
#include <optional>
#include <semaphore>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include "atom/macro.hpp"
#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

namespace atom::async {

/// @brief 异常类：线程池错误
class ThreadPoolError : public std::runtime_error {
public:
    explicit ThreadPoolError(const std::string& msg)
        : std::runtime_error(msg) {}
    explicit ThreadPoolError(const char* msg) : std::runtime_error(msg) {}
};

/**
 * @brief 改进的概念用于定义可锁定类型
 * @details 基于C++标准中的Lockable和BasicLockable概念
 * @see https://en.cppreference.com/w/cpp/named_req/Lockable
 */
template <typename Lock>
concept is_lockable = requires(Lock lock) {
    { lock.lock() } -> std::same_as<void>;
    { lock.unlock() } -> std::same_as<void>;
    { lock.try_lock() } -> std::same_as<bool>;
};

/**
 * @brief 线程安全队列，用于管理多线程环境下的数据访问
 * @tparam T 队列中元素的类型
 * @tparam Lock 锁类型，默认为std::mutex
 */
template <typename T, typename Lock = std::mutex>
    requires is_lockable<Lock>
class ThreadSafeQueue {
public:
    using value_type = T;
    using size_type = typename std::deque<T>::size_type;
    static constexpr size_type max_size = std::numeric_limits<size_type>::max();

    ThreadSafeQueue() = default;

    // Copy constructor
    ThreadSafeQueue(const ThreadSafeQueue& other) {
        try {
            std::scoped_lock lock(other.mutex_);
            data_ = other.data_;
        } catch (const std::exception& e) {
            throw ThreadPoolError(std::string("Copy constructor failed: ") +
                                  e.what());
        }
    }

    // Copy assignment operator
    auto operator=(const ThreadSafeQueue& other) -> ThreadSafeQueue& {
        if (this != &other) {
            try {
                std::scoped_lock lockThis(mutex_, std::defer_lock);
                std::scoped_lock lockOther(other.mutex_, std::defer_lock);
                std::lock(lockThis, lockOther);
                data_ = other.data_;
            } catch (const std::exception& e) {
                throw ThreadPoolError(std::string("Copy assignment failed: ") +
                                      e.what());
            }
        }
        return *this;
    }

    // Move constructor
    ThreadSafeQueue(ThreadSafeQueue&& other) noexcept {
        try {
            std::scoped_lock lock(other.mutex_);
            data_ = std::move(other.data_);
        } catch (...) {
            // 保持强异常安全性，确保即使在异常情况下对象也是有效的
        }
    }

    // Move assignment operator
    auto operator=(ThreadSafeQueue&& other) noexcept -> ThreadSafeQueue& {
        if (this != &other) {
            try {
                std::scoped_lock lockThis(mutex_, std::defer_lock);
                std::scoped_lock lockOther(other.mutex_, std::defer_lock);
                std::lock(lockThis, lockOther);
                data_ = std::move(other.data_);
            } catch (...) {
                // 保持强异常安全性
            }
        }
        return *this;
    }

    /**
     * @brief 将元素添加到队列尾部
     * @param value 要添加的元素
     * @throws ThreadPoolError 如果队列已满或添加失败
     */
    void pushBack(T&& value) {
        std::scoped_lock lock(mutex_);
        if (data_.size() >= max_size) {
            throw ThreadPoolError("Queue is full");
        }
        try {
            data_.push_back(std::forward<T>(value));
        } catch (const std::exception& e) {
            throw ThreadPoolError(std::string("Push back failed: ") + e.what());
        }
    }

    /**
     * @brief 将元素添加到队列头部
     * @param value 要添加的元素
     * @throws ThreadPoolError 如果队列已满或添加失败
     */
    void pushFront(T&& value) {
        std::scoped_lock lock(mutex_);
        if (data_.size() >= max_size) {
            throw ThreadPoolError("Queue is full");
        }
        try {
            data_.push_front(std::forward<T>(value));
        } catch (const std::exception& e) {
            throw ThreadPoolError(std::string("Push front failed: ") +
                                  e.what());
        }
    }

    /**
     * @brief 检查队列是否为空
     * @return 如果队列为空返回true，否则返回false
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        try {
            std::scoped_lock lock(mutex_);
            return data_.empty();
        } catch (...) {
            return true;  // 保守返回空
        }
    }

    /**
     * @brief 获取队列中元素的数量
     * @return 队列中元素的数量
     */
    [[nodiscard]] auto size() const noexcept -> size_type {
        try {
            std::scoped_lock lock(mutex_);
            return data_.size();
        } catch (...) {
            return 0;  // 保守返回0
        }
    }

    /**
     * @brief 弹出队列前面的元素
     * @return 如果队列不为空，返回队列前面的元素；否则返回std::nullopt
     */
    [[nodiscard]] auto popFront() noexcept -> std::optional<T> {
        try {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) {
                return std::nullopt;
            }

            auto front = std::move(data_.front());
            data_.pop_front();
            return front;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief 弹出队列后面的元素
     * @return 如果队列不为空，返回队列后面的元素；否则返回std::nullopt
     */
    [[nodiscard]] auto popBack() noexcept -> std::optional<T> {
        try {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) {
                return std::nullopt;
            }

            auto back = std::move(data_.back());
            data_.pop_back();
            return back;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief 从队列中偷取一个元素（通常用于工作窃取调度）
     * @return 如果队列不为空，返回队列后面的元素；否则返回std::nullopt
     */
    [[nodiscard]] auto steal() noexcept -> std::optional<T> {
        try {
            std::scoped_lock lock(mutex_);
            if (data_.empty()) {
                return std::nullopt;
            }

            auto back = std::move(data_.back());
            data_.pop_back();
            return back;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief 将指定项移动到队列前面
     * @param item 要移动的项
     */
    void rotateToFront(const T& item) noexcept {
        try {
            std::scoped_lock lock(mutex_);
            // 使用C++20 ranges查找元素
            auto iter = std::ranges::find(data_, item);

            if (iter != data_.end()) {
                std::ignore = data_.erase(iter);
            }

            data_.push_front(item);
        } catch (...) {
            // 保持操作的原子性，确保不会导致数据损坏
        }
    }

    /**
     * @brief 复制队列前面的元素并将其移动到队尾
     * @return 如果队列非空，返回前面的元素的复制；否则返回std::nullopt
     */
    [[nodiscard]] auto copyFrontAndRotateToBack() noexcept -> std::optional<T> {
        try {
            std::scoped_lock lock(mutex_);

            if (data_.empty()) {
                return std::nullopt;
            }

            auto front = data_.front();
            data_.pop_front();

            data_.push_back(front);

            return front;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief 清空队列
     */
    void clear() noexcept {
        try {
            std::scoped_lock lock(mutex_);
            data_.clear();
        } catch (...) {
            // 尝试清空失败时忽略异常
        }
    }

private:
    std::deque<T> data_;
    mutable Lock mutex_;
};

namespace details {
#ifdef __cpp_lib_move_only_function
using default_function_type = std::move_only_function<void()>;
#else
using default_function_type = std::function<void()>;
#endif
}  // namespace details

/**
 * @brief 增强的线程池实现，支持工作窃取和优先级调度
 * @tparam FunctionType 任务函数类型
 * @tparam ThreadType 线程类型，默认为std::jthread
 */
template <typename FunctionType = details::default_function_type,
          typename ThreadType = std::jthread>
    requires std::invocable<FunctionType> &&
             std::is_same_v<void, std::invoke_result_t<FunctionType>>
class ThreadPool {
public:
    /**
     * @brief 构造函数
     * @param number_of_threads 线程数，默认为系统硬件支持的线程数
     * @param init 线程初始化函数，在每个线程启动时执行
     * @throws ThreadPoolError 如果初始化失败
     */
    template <
        typename InitializationFunction = std::function<void(std::size_t)>>
        requires std::invocable<InitializationFunction, std::size_t> &&
                 std::is_same_v<void, std::invoke_result_t<
                                          InitializationFunction, std::size_t>>
    explicit ThreadPool(
        const unsigned int& number_of_threads =
            std::thread::hardware_concurrency(),
        InitializationFunction init = [](std::size_t) {})
        : tasks_(validateThreadCount(number_of_threads)) {
        try {
            std::size_t currentId = 0;
            for (std::size_t i = 0; i < tasks_.size(); ++i) {
                priority_queue_.pushBack(std::move(currentId));
                try {
                    threads_.emplace_back(
                        [this, threadId = currentId, init = std::move(init)](
                            const std::stop_token& stop_tok) {
                            threadFunction(threadId, init, stop_tok);
                        });
                    ++currentId;
                } catch (const std::exception& e) {
                    tasks_.pop_back();
                    std::ignore = priority_queue_.popBack();
                    throw ThreadPoolError(
                        std::string("Failed to create thread: ") + e.what());
                }
            }
        } catch (const std::exception& e) {
            // 清理已创建的资源
            shutdown();
            throw ThreadPoolError(
                std::string("Thread pool initialization failed: ") + e.what());
        }
    }

    /**
     * @brief 析构函数，等待所有任务完成并停止所有线程
     */
    ~ThreadPool() noexcept { shutdown(); }

    // 删除复制构造函数和复制赋值运算符
    ThreadPool(const ThreadPool&) = delete;
    auto operator=(const ThreadPool&) -> ThreadPool& = delete;

    // 定义移动构造函数和移动赋值运算符
    ThreadPool(ThreadPool&& other) noexcept = default;
    auto operator=(ThreadPool&& other) noexcept -> ThreadPool& = default;

    /**
     * @brief 向线程池提交任务并返回future以获取结果
     * @tparam Function 函数类型
     * @tparam Args 函数参数类型
     * @tparam ReturnType 函数返回类型
     * @param func 要执行的函数
     * @param args 函数参数
     * @return std::future，用于获取任务结果
     * @throws ThreadPoolError 如果任务提交失败
     */
    template <typename Function, typename... Args,
              typename ReturnType = std::invoke_result_t<Function&&, Args&&...>>
        requires std::invocable<Function, Args...>
    [[nodiscard]] auto enqueue(Function func,
                               Args... args) -> std::future<ReturnType> {
        if (is_shutdown_.load(std::memory_order_acquire)) {
            throw ThreadPoolError(
                "Cannot enqueue task: Thread pool is shutting down");
        }

#ifdef __cpp_lib_move_only_function
        std::promise<ReturnType> promise;
        auto future = promise.get_future();
        auto task = [func = std::move(func), ... largs = std::move(args),
                     promise = std::move(promise)]() mutable {
            try {
                if constexpr (std::is_same_v<ReturnType, void>) {
                    std::invoke(func, largs...);
                    promise.set_value();
                } else {
                    promise.set_value(std::invoke(func, largs...));
                }
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        };
        try {
            enqueueTask(std::move(task));
        } catch (const std::exception& e) {
            throw ThreadPoolError(std::string("Failed to enqueue task: ") +
                                  e.what());
        }
        return future;
#else
        auto shared_promise = std::make_shared<std::promise<ReturnType>>();
        auto task = [func = std::move(func), ... largs = std::move(args),
                     promise = shared_promise]() {
            try {
                if constexpr (std::is_same_v<ReturnType, void>) {
                    std::invoke(func, largs...);
                    promise->set_value();
                } else {
                    promise->set_value(std::invoke(func, largs...));
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        };

        auto future = shared_promise->get_future();
        try {
            enqueueTask(std::move(task));
        } catch (const std::exception& e) {
            throw ThreadPoolError(std::string("Failed to enqueue task: ") +
                                  e.what());
        }
        return future;
#endif
    }

    /**
     * @brief 提交任务但不返回future（不关心结果）
     * @tparam Function 函数类型
     * @tparam Args 函数参数类型
     * @param func 要执行的函数
     * @param args 函数参数
     * @throws ThreadPoolError 如果任务提交失败
     */
    template <typename Function, typename... Args>
        requires std::invocable<Function, Args...>
    void enqueueDetach(Function&& func, Args&&... args) {
        if (is_shutdown_.load(std::memory_order_acquire)) {
            throw ThreadPoolError(
                "Cannot enqueue detached task: Thread pool is shutting down");
        }

        try {
            enqueueTask([func = std::forward<Function>(func),
                         ... largs = std::forward<Args>(args)]() mutable {
                try {
                    if constexpr (std::is_same_v<
                                      void, std::invoke_result_t<Function&&,
                                                                 Args&&...>>) {
                        std::invoke(func, largs...);
                    } else {
                        std::ignore = std::invoke(func, largs...);
                    }
                } catch (...) {
                    // 捕获并记录异常（在生产环境中可能会记录到日志）
                }
            });
        } catch (const std::exception& e) {
            throw ThreadPoolError(
                std::string("Failed to enqueue detached task: ") + e.what());
        }
    }

    /**
     * @brief 获取线程池中的线程数
     * @return 线程数
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return threads_.size();
    }

    /**
     * @brief 等待所有任务完成
     * @param timeout_ms 等待超时时间（毫秒），0表示一直等待
     * @return 如果所有任务完成返回true，如果超时返回false
     */
    bool waitForTasks(std::size_t timeout_ms = 0) noexcept {
        try {
            if (in_flight_tasks_.load(std::memory_order_acquire) > 0) {
                if (timeout_ms == 0) {
                    threads_complete_signal_.wait(false);
                    return true;
                } else {
                    // 使用超时版本的wait
                    auto deadline = std::chrono::steady_clock::now() +
                                    std::chrono::milliseconds(timeout_ms);
                    while (!threads_complete_signal_.load(
                               std::memory_order_acquire) &&
                           std::chrono::steady_clock::now() < deadline) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(1));
                    }
                    return threads_complete_signal_.load(
                        std::memory_order_acquire);
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief 批量提交任务并等待所有任务完成
     * @tparam Iterator 迭代器类型
     * @param begin 任务范围起始迭代器
     * @param end 任务范围结束迭代器
     * @return 是否所有任务都成功提交并完成
     */
    template <typename Iterator>
    bool submitBatch(Iterator begin, Iterator end) {
        try {
            std::vector<std::future<void>> futures;
            futures.reserve(std::distance(begin, end));

            for (auto it = begin; it != end; ++it) {
                futures.push_back(enqueue(*it));
            }

            for (auto& future : futures) {
                future.wait();
            }

            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    /**
     * @brief 获取当前活跃的任务数
     * @return 活跃任务数
     */
    [[nodiscard]] auto activeTaskCount() const noexcept -> std::size_t {
        return in_flight_tasks_.load(std::memory_order_acquire);
    }

    /**
     * @brief 检查线程池是否正在关闭
     * @return 如果线程池正在关闭返回true
     */
    [[nodiscard]] bool isShuttingDown() const noexcept {
        return is_shutdown_.load(std::memory_order_acquire);
    }

private:
    /**
     * @brief 验证并返回有效的线程数
     * @param thread_count 请求的线程数
     * @return 有效的线程数
     */
    static unsigned int validateThreadCount(unsigned int thread_count) {
        const unsigned int min_threads = 1;
        const unsigned int max_threads = 256;  // 设置一个合理的上限
        const unsigned int default_threads =
            std::thread::hardware_concurrency();

        if (thread_count < min_threads) {
            return min_threads;
        } else if (thread_count > max_threads) {
            return max_threads;
        } else if (thread_count == 0) {
            return default_threads > 0 ? default_threads : min_threads;
        }

        return thread_count;
    }

    /**
     * @brief 关闭线程池，等待任务完成并停止所有线程
     */
    void shutdown() noexcept {
        is_shutdown_.store(true, std::memory_order_release);

        try {
            waitForTasks();

            for (auto& thread : threads_) {
                if (thread.joinable()) {
                    thread.request_stop();
                }
            }

            for (auto& task : tasks_) {
                task.signal.release();
            }

            for (auto& thread : threads_) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
        } catch (...) {
            // 处理关闭过程中的异常
        }
    }

    /**
     * @brief 线程工作函数
     * @param threadId 线程ID
     * @param init 初始化函数
     * @param stop_tok 停止令牌
     */
    template <typename InitFunc>
    void threadFunction(std::size_t threadId, InitFunc& init,
                        const std::stop_token& stop_tok) noexcept {
        try {
            std::invoke(init, threadId);
        } catch (...) {
            // 初始化失败但继续执行
        }

        do {
            tasks_[threadId].signal.acquire();

            do {
                processTasksFromQueue(threadId);
                stealAndProcessTasks(threadId);
            } while (unassigned_tasks_.load(std::memory_order_acquire) > 0);

            priority_queue_.rotateToFront(threadId);

            if (in_flight_tasks_.load(std::memory_order_acquire) == 0) {
                threads_complete_signal_.store(true, std::memory_order_release);
                threads_complete_signal_.notify_one();
            }

        } while (!stop_tok.stop_requested() &&
                 !is_shutdown_.load(std::memory_order_acquire));
    }

    /**
     * @brief 处理线程自己队列中的任务
     * @param threadId 线程ID
     */
    void processTasksFromQueue(std::size_t threadId) noexcept {
        while (auto task = tasks_[threadId].tasks.popFront()) {
            unassigned_tasks_.fetch_sub(1, std::memory_order_release);
            try {
                std::invoke(std::move(task.value()));
            } catch (...) {
                // 捕获任务执行异常
            }
            in_flight_tasks_.fetch_sub(1, std::memory_order_release);
        }
    }

    /**
     * @brief 从其他线程队列窃取并执行任务
     * @param threadId 当前线程ID
     */
    void stealAndProcessTasks(std::size_t threadId) noexcept {
        for (std::size_t j = 1; j < tasks_.size(); ++j) {
            const std::size_t INDEX = (threadId + j) % tasks_.size();
            if (auto task = tasks_[INDEX].tasks.steal()) {
                unassigned_tasks_.fetch_sub(1, std::memory_order_release);
                try {
                    std::invoke(std::move(task.value()));
                } catch (...) {
                    // 捕获任务执行异常
                }
                in_flight_tasks_.fetch_sub(1, std::memory_order_release);
                break;
            }
        }
    }

    /**
     * @brief 将任务放入队列中
     * @tparam Function 任务函数类型
     * @param func 任务函数
     * @throws ThreadPoolError 如果无法提交任务
     */
    template <typename Function>
    void enqueueTask(Function&& func) {
        if (is_shutdown_.load(std::memory_order_acquire)) {
            throw ThreadPoolError("Thread pool is shutting down");
        }

        auto iOpt = priority_queue_.copyFrontAndRotateToBack();
        if (!iOpt.has_value()) {
            throw ThreadPoolError(
                "Failed to get thread index from priority queue");
        }
        auto index = *(iOpt);

        unassigned_tasks_.fetch_add(1, std::memory_order_release);
        const auto PREV_IN_FLIGHT =
            in_flight_tasks_.fetch_add(1, std::memory_order_release);

        if (PREV_IN_FLIGHT == 0) {
            threads_complete_signal_.store(false, std::memory_order_release);
        }

        try {
            tasks_[index].tasks.pushBack(std::forward<Function>(func));
            tasks_[index].signal.release();
        } catch (...) {
            // 如果失败，回滚计数器
            unassigned_tasks_.fetch_sub(1, std::memory_order_release);
            in_flight_tasks_.fetch_sub(1, std::memory_order_release);
            throw ThreadPoolError("Failed to enqueue task");
        }
    }

    struct TaskItem {
        atom::async::ThreadSafeQueue<FunctionType> tasks{};
        std::binary_semaphore signal{0};
    } ATOM_ALIGNAS(128);  // 使用缓存行对齐避免假共享

    std::vector<ThreadType> threads_;
    std::deque<TaskItem> tasks_;
    atom::async::ThreadSafeQueue<std::size_t> priority_queue_;
    std::atomic_int_fast64_t unassigned_tasks_{0}, in_flight_tasks_{0};
    std::atomic_bool threads_complete_signal_{false};
    std::atomic_bool is_shutdown_{false};  // 新增：线程池关闭标志
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_POOL_HPP

/*
 * fifo_threadpool.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Thread pool for async FIFO operations

*************************************************/

#ifndef ATOM_CONNECTION_FIFO_THREADPOOL_HPP
#define ATOM_CONNECTION_FIFO_THREADPOOL_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace atom::connection {

/**
 * @brief A simple thread pool for managing async FIFO operations
 *
 * This class provides a shared thread pool to avoid creating detached threads
 * for each async operation. It ensures proper lifecycle management and resource
 * cleanup.
 */
class FifoThreadPool {
public:
    /**
     * @brief Get the singleton instance of the thread pool
     */
    static FifoThreadPool& instance() {
        static FifoThreadPool pool;
        return pool;
    }

    /**
     * @brief Submit a task to the thread pool
     *
     * @tparam F Callable type
     * @tparam Args Argument types
     * @param f Callable to execute
     * @param args Arguments to pass to the callable
     * @return std::future with the result of the callable
     */
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("Cannot submit to stopped thread pool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }

        condition_.notify_one();
        return result;
    }

    /**
     * @brief Submit a void task (fire-and-forget style but managed)
     *
     * @param task Task to execute
     */
    void submitVoid(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                return;
            }
            tasks_.emplace(std::move(task));
        }
        condition_.notify_one();
    }

    /**
     * @brief Get the number of worker threads
     */
    [[nodiscard]] size_t size() const noexcept { return workers_.size(); }

    /**
     * @brief Get the number of pending tasks
     */
    [[nodiscard]] size_t pendingTasks() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

    // Non-copyable, non-movable
    FifoThreadPool(const FifoThreadPool&) = delete;
    FifoThreadPool& operator=(const FifoThreadPool&) = delete;
    FifoThreadPool(FifoThreadPool&&) = delete;
    FifoThreadPool& operator=(FifoThreadPool&&) = delete;

private:
    FifoThreadPool(size_t num_threads = 0) : stop_(false) {
        if (num_threads == 0) {
            num_threads = std::max(2u, std::thread::hardware_concurrency());
        }

        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ~FifoThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void workerLoop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });

                if (stop_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            try {
                task();
            } catch (...) {
                // Silently ignore exceptions in worker threads
                // Individual tasks should handle their own exceptions
            }
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
};

/**
 * @brief Helper function to submit a task to the global thread pool
 */
template <typename F, typename... Args>
auto submitToPool(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {
    return FifoThreadPool::instance().submit(std::forward<F>(f),
                                             std::forward<Args>(args)...);
}

/**
 * @brief Helper function to submit a void task to the global thread pool
 */
inline void submitVoidToPool(std::function<void()> task) {
    FifoThreadPool::instance().submitVoid(std::move(task));
}

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_FIFO_THREADPOOL_HPP

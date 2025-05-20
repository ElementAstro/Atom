/*
 * timer.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-14

Description: Timer class for C++

**************************************************/

#ifndef ATOM_ASYNC_TIMER_HPP
#define ATOM_ASYNC_TIMER_HPP

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#endif

#include "future.hpp"

namespace atom::async {

template <typename F, typename... Args>
concept Invocable = requires(F &&f, Args &&...args) {
    std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
};

/**
 * @brief Represents a task to be scheduled and executed by the Timer.
 */
class TimerTask {
public:
    /**
     * @brief Constructor for TimerTask.
     *
     * @param func The function to be executed when the task runs.
     * @param delay The delay in milliseconds before the first execution.
     * @param repeatCount The number of times the task should be repeated. -1
     * for infinite repetition.
     * @param priority The priority of the task.
     * @throws std::invalid_argument If func is null or delay is invalid
     */
    explicit TimerTask(std::function<void()> func, unsigned int delay,
                       int repeatCount, int priority) noexcept(false);

    /**
     * @brief Comparison operator for comparing two TimerTask objects based on
     * their next execution time.
     *
     * @param other Another TimerTask object to compare to.
     * @return True if this task's next execution time is earlier than the other
     * task's next execution time.
     */
    [[nodiscard]] auto operator<(const TimerTask &other) const noexcept -> bool;

    /**
     * @brief Executes the task's associated function.
     * @throws Propagates any exceptions thrown by the task function
     */
    void run() noexcept(false);

    /**
     * @brief Get the next scheduled execution time of the task.
     *
     * @return The steady clock time point representing the next execution time.
     */
    [[nodiscard]] auto getNextExecutionTime() const noexcept
        -> std::chrono::steady_clock::time_point;

    std::function<void()> m_func;  ///< The function to be executed.
    unsigned int m_delay;          ///< The delay before the first execution.
    int m_repeatCount;             ///< The number of repetitions remaining.
    int m_priority;                ///< The priority of the task.
    std::chrono::steady_clock::time_point
        m_nextExecutionTime;  ///< The next execution time.
};

/**
 * @brief Represents a timer for scheduling and executing tasks.
 */
class Timer {
public:
    /**
     * @brief Constructor for Timer.
     */
    Timer() noexcept(false);

    /**
     * @brief Destructor for Timer.
     */
    ~Timer() noexcept;

    Timer(const Timer &) = delete;
    Timer &operator=(const Timer &) = delete;
    Timer(Timer &&) = delete;
    Timer &operator=(Timer &&) = delete;

    /**
     * @brief Schedules a task to be executed once after a specified delay.
     *
     * @tparam Function The type of the function to be executed.
     * @tparam Args The types of the arguments to be passed to the function.
     * @param func The function to be executed.
     * @param delay The delay in milliseconds before the function is executed.
     * @param args The arguments to be passed to the function.
     * @return An EnhancedFuture representing the result of the function
     * execution.
     * @throws std::invalid_argument If the function is null or delay is invalid
     */
    template <typename Function, typename... Args>
        requires Invocable<Function, Args...>
    [[nodiscard]] auto setTimeout(Function &&func, unsigned int delay,
                                  Args &&...args) noexcept(false)
        -> EnhancedFuture<std::invoke_result_t<Function, Args...>>;

    /**
     * @brief Schedules a task to be executed repeatedly at a specified
     * interval.
     *
     * @tparam Function The type of the function to be executed.
     * @tparam Args The types of the arguments to be passed to the function.
     * @param func The function to be executed.
     * @param interval The interval in milliseconds between executions.
     * @param repeatCount The number of times the function should be repeated.
     * -1 for infinite repetition.
     * @param priority The priority of the task.
     * @param args The arguments to be passed to the function.
     * @throws std::invalid_argument If func is null, interval is 0, or
     * repeatCount is < -1
     */
    template <typename Function, typename... Args>
        requires Invocable<Function, Args...>
    void setInterval(Function &&func, unsigned int interval, int repeatCount,
                     int priority, Args &&...args) noexcept(false);

    [[nodiscard]] auto now() const noexcept
        -> std::chrono::steady_clock::time_point;

    /**
     * @brief Cancels all scheduled tasks.
     */
    void cancelAllTasks() noexcept;

    /**
     * @brief Pauses the execution of scheduled tasks.
     */
    void pause() noexcept;

    /**
     * @brief Resumes the execution of scheduled tasks after pausing.
     */
    void resume() noexcept;

    /**
     * @brief Stops the timer and cancels all tasks.
     */
    void stop() noexcept;

    /**
     * @brief Blocks the calling thread until all tasks are completed.
     */
    void wait() noexcept;

    /**
     * @brief Sets a callback function to be called when a task is executed.
     *
     * @tparam Function The type of the callback function.
     * @param func The callback function to be set.
     * @throws std::invalid_argument If the function is null
     */
    template <typename Function>
        requires Invocable<Function>
    void setCallback(Function &&func) noexcept(false);

    [[nodiscard]] auto getTaskCount() const noexcept -> size_t;

private:
    /**
     * @brief Adds a task to the task queue.
     *
     * @tparam Function The type of the function to be executed.
     * @tparam Args The types of the arguments to be passed to the function.
     * @param func The function to be executed.
     * @param delay The delay in milliseconds before the function is executed.
     * @param repeatCount The number of repetitions remaining.
     * @param priority The priority of the task.
     * @param args The arguments to be passed to the function.
     * @return An EnhancedFuture representing the result of the function
     * execution.
     * @throws std::invalid_argument If func is null or parameters are invalid
     */
    template <typename Function, typename... Args>
        requires Invocable<Function, Args...>
    auto addTask(Function &&func, unsigned int delay, int repeatCount,
                 int priority, Args &&...args) noexcept(false)
        -> EnhancedFuture<std::invoke_result_t<Function, Args...>>;

    /**
     * @brief Main execution loop for processing and running tasks.
     */
    void run() noexcept;

    /**
     * @brief Validates task parameters
     *
     * @param delay The delay value to validate
     * @param repeatCount The repeat count to validate
     * @throws std::invalid_argument If parameters are invalid
     */
    static void validateTaskParams(unsigned int delay,
                                   int repeatCount) noexcept(false);

    std::jthread m_thread;  ///< The thread for running the timer loop (C++20)

#ifdef ATOM_USE_BOOST_LOCKFREE
    /**
     * @brief Task container using Boost.lockfree for better performance in
     * high-concurrency scenarios
     */
    class TaskContainer {
    public:
        TaskContainer() : m_queue(128) {}  // Default capacity of 128 tasks

        void push(const TimerTask &task) {
            // Create a copy on heap since lockfree queue needs ownership
            auto *taskPtr = new TimerTask(task);
            // Try pushing until successful
            while (!m_queue.push(taskPtr)) {
                // If queue is full, allow other threads to process
                std::this_thread::yield();
            }
        }

        bool pop(TimerTask &task) {
            TimerTask *taskPtr = nullptr;
            if (m_queue.pop(taskPtr)) {
                if (taskPtr) {
                    task = *taskPtr;
                    delete taskPtr;
                    return true;
                }
            }
            return false;
        }

        bool empty() const { return m_queue.empty(); }

        void clear() {
            TimerTask *taskPtr = nullptr;
            while (m_queue.pop(taskPtr)) {
                if (taskPtr) {
                    delete taskPtr;
                }
            }
        }

        ~TaskContainer() { clear(); }

    private:
        boost::lockfree::queue<TimerTask *> m_queue;
    };

    TaskContainer m_taskContainer;  ///< Lockfree container for pending tasks
    TimerTask m_currentTask;        ///< The current task being processed
    std::atomic<bool> m_hasCurrentTask{
        false};  ///< Flag indicating if there's a current task
#else
    std::priority_queue<TimerTask>
        m_taskQueue;  ///< The priority queue for scheduled tasks.
#endif

    mutable std::mutex m_mutex;  ///< Mutex for thread synchronization.
    std::condition_variable
        m_cond;  ///< Condition variable for thread synchronization.
    std::function<void()> m_callback;  ///< The callback function to be called
                                       ///< when a task is executed.
    std::atomic<bool> m_stop{
        false};  ///< Flag indicating whether the timer should stop.
    std::atomic<bool> m_paused{
        false};  ///< Flag indicating whether the timer is paused.
};

template <typename Function, typename... Args>
    requires Invocable<Function, Args...>
auto Timer::setTimeout(Function &&func, unsigned int delay,
                       Args &&...args) noexcept(false)
    -> EnhancedFuture<std::invoke_result_t<Function, Args...>> {
    validateTaskParams(delay, 1);

    using ReturnType = std::invoke_result_t<Function, Args...>;
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<Function>(func), std::forward<Args>(args)...));
    std::future<ReturnType> result = task->get_future();

    {
        std::scoped_lock lock(m_mutex);
        m_taskQueue.emplace([task]() { (*task)(); }, delay, 1, 0);
    }

    m_cond.notify_all();
    return EnhancedFuture<ReturnType>(std::move(result).share());
}

template <typename Function, typename... Args>
    requires Invocable<Function, Args...>
void Timer::setInterval(Function &&func, unsigned int interval, int repeatCount,
                        int priority, Args &&...args) noexcept(false) {
    // 移除对func的空检查
    if (interval == 0) {
        throw std::invalid_argument(
            "Timer::setInterval: Interval must be greater than 0");
    }
    validateTaskParams(interval, repeatCount);

    addTask(std::forward<Function>(func), interval, repeatCount, priority,
            std::forward<Args>(args)...);
}

template <typename Function, typename... Args>
    requires Invocable<Function, Args...>
auto Timer::addTask(Function &&func, unsigned int delay, int repeatCount,
                    int priority, Args &&...args) noexcept(false)
    -> EnhancedFuture<std::invoke_result_t<Function, Args...>> {
    // 移除对func的空检查
    validateTaskParams(delay, repeatCount);

    using ReturnType = std::invoke_result_t<Function, Args...>;
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<Function>(func), std::forward<Args>(args)...));
    std::future<ReturnType> result = task->get_future();

    TimerTask timerTask([task]() { (*task)(); }, delay, repeatCount, priority);

#ifdef ATOM_USE_BOOST_LOCKFREE
    // For lockfree implementation, we don't need lock for pushing task
    m_taskContainer.push(timerTask);
#else
    {
        std::scoped_lock lock(m_mutex);
        m_taskQueue.emplace([task]() { (*task)(); }, delay, repeatCount,
                            priority);
    }
#endif

    m_cond.notify_all();
    return EnhancedFuture<ReturnType>(std::move(result).share());
}

template <typename Function>
    requires Invocable<Function>
void Timer::setCallback(Function &&func) noexcept(false) {
    std::scoped_lock lock(m_mutex);
    m_callback = std::forward<Function>(func);
}

}  // namespace atom::async

#endif
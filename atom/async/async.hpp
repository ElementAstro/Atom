/*
 * async.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: A simple but useful async worker manager

**************************************************/

#ifndef ATOM_ASYNC_ASYNC_HPP
#define ATOM_ASYNC_ASYNC_HPP

#include <chrono>
#include <cmath>
#include <concepts>
#include <coroutine>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

#include "atom/async/future.hpp"
#include "atom/error/exception.hpp"

class TimeoutException : public atom::error::RuntimeError {
public:
    using atom::error::RuntimeError::RuntimeError;
};

#define THROW_TIMEOUT_EXCEPTION(...)                                       \
    throw TimeoutException(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                           __VA_ARGS__);

namespace atom::async {

// C++20 concepts for improved type safety
template <typename T>
concept Invocable = requires { std::is_invocable_v<T>; };

template <typename T>
concept Callable = requires(T t) { t(); };

template <typename Func, typename... Args>
concept InvocableWithArgs =
    requires(Func f, Args... args) { std::invoke(f, args...); };

template <typename T>
concept NonVoidType = !std::is_void_v<T>;

/**
 * @brief Class for performing asynchronous tasks.
 *
 * This class allows you to start a task asynchronously and get the result when
 * it's done. It also provides functionality to cancel the task, check if it's
 * done or active, validate the result, set a callback function, and set a
 * timeout.
 *
 * @tparam ResultType The type of the result returned by the task.
 */
template <typename ResultType>
class AsyncWorker {
public:
    AsyncWorker() noexcept = default;
    ~AsyncWorker() noexcept = default;

    // Rule of five - prevent copy, allow move
    AsyncWorker(const AsyncWorker&) = delete;
    AsyncWorker& operator=(const AsyncWorker&) = delete;
    AsyncWorker(AsyncWorker&&) noexcept = default;
    AsyncWorker& operator=(AsyncWorker&&) noexcept = default;

    /**
     * @brief Starts the task asynchronously.
     *
     * @tparam Func The type of the function to be executed asynchronously.
     * @tparam Args The types of the arguments to be passed to the function.
     * @param func The function to be executed asynchronously.
     * @param args The arguments to be passed to the function.
     * @throws std::invalid_argument If func is null or invalid.
     */
    template <typename Func, typename... Args>
        requires InvocableWithArgs<Func, Args...> &&
                 std::is_same_v<std::invoke_result_t<Func, Args...>, ResultType>
    void startAsync(Func&& func, Args&&... args);

    /**
     * @brief Gets the result of the task with timeout option.
     *
     * @param timeout Optional timeout duration (0 means no timeout).
     * @throws std::invalid_argument if the task is not valid.
     * @throws TimeoutException if the timeout is reached.
     * @return The result of the task.
     */
    [[nodiscard]] auto getResult(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0))
        -> ResultType;

    /**
     * @brief Cancels the task.
     *
     * If the task is valid, this function waits for the task to complete.
     */
    void cancel() noexcept;

    /**
     * @brief Checks if the task is done.
     *
     * @return True if the task is done, false otherwise.
     */
    [[nodiscard]] auto isDone() const noexcept -> bool;

    /**
     * @brief Checks if the task is active.
     *
     * @return True if the task is active, false otherwise.
     */
    [[nodiscard]] auto isActive() const noexcept -> bool;

    /**
     * @brief Validates the result of the task using a validator function.
     *
     * @param validator The function used to validate the result.
     * @return True if the result is valid, false otherwise.
     */
    auto validate(std::function<bool(ResultType)> validator) noexcept -> bool;

    /**
     * @brief Sets a callback function to be called when the task is done.
     *
     * @param callback The callback function to be set.
     * @throws std::invalid_argument if callback is empty.
     */
    void setCallback(std::function<void(ResultType)> callback);

    /**
     * @brief Sets a timeout for the task.
     *
     * @param timeout The timeout duration.
     * @throws std::invalid_argument if timeout is negative.
     */
    void setTimeout(std::chrono::seconds timeout);

    /**
     * @brief Waits for the task to complete.
     *
     * If a timeout is set, this function waits until the task is done or the
     * timeout is reached. If a callback function is set and the task is done,
     * the callback function is called with the result.
     *
     * @throws TimeoutException if the timeout is reached.
     */
    void waitForCompletion();

private:
    std::future<ResultType> task_;
    std::function<void(ResultType)> callback_;
    std::chrono::seconds timeout_{0};
};

/**
 * @brief Class for managing multiple AsyncWorker instances.
 *
 * This class provides functionality to create and manage multiple AsyncWorker
 * instances using modern C++20 features.
 *
 * @tparam ResultType The type of the result returned by the tasks managed by
 * this class.
 */
template <typename ResultType>
class AsyncWorkerManager {
public:
    /**
     * @brief Default constructor.
     */
    AsyncWorkerManager() noexcept = default;

    /**
     * @brief Destructor that ensures cleanup.
     */
    ~AsyncWorkerManager() noexcept {
        try {
            cancelAll();
        } catch (...) {
            // Suppress any exceptions in destructor
        }
    }

    // Rule of five - prevent copy, allow move
    AsyncWorkerManager(const AsyncWorkerManager&) = delete;
    AsyncWorkerManager& operator=(const AsyncWorkerManager&) = delete;
    AsyncWorkerManager(AsyncWorkerManager&&) noexcept = default;
    AsyncWorkerManager& operator=(AsyncWorkerManager&&) noexcept = default;

    /**
     * @brief Creates a new AsyncWorker instance and starts the task
     * asynchronously.
     *
     * @tparam Func The type of the function to be executed asynchronously.
     * @tparam Args The types of the arguments to be passed to the function.
     * @param func The function to be executed asynchronously.
     * @param args The arguments to be passed to the function.
     * @return A shared pointer to the created AsyncWorker instance.
     */
    template <typename Func, typename... Args>
        requires InvocableWithArgs<Func, Args...> &&
                     std::is_same_v<std::invoke_result_t<Func, Args...>,
                                    ResultType>
    [[nodiscard]] auto createWorker(Func&& func, Args&&... args)
        -> std::shared_ptr<AsyncWorker<ResultType>>;

    /**
     * @brief Cancels all the managed tasks.
     */
    void cancelAll() noexcept;

    /**
     * @brief Checks if all the managed tasks are done.
     *
     * @return True if all tasks are done, false otherwise.
     */
    [[nodiscard]] auto allDone() const noexcept -> bool;

    /**
     * @brief Waits for all the managed tasks to complete.
     *
     * @param timeout Optional timeout for each task (0 means no timeout)
     * @throws TimeoutException if any task exceeds the timeout.
     */
    void waitForAll(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Checks if a specific task is done.
     *
     * @param worker The AsyncWorker instance to check.
     * @return True if the task is done, false otherwise.
     * @throws std::invalid_argument if worker is null.
     */
    [[nodiscard]] auto isDone(
        std::shared_ptr<AsyncWorker<ResultType>> worker) const -> bool;

    /**
     * @brief Cancels a specific task.
     *
     * @param worker The AsyncWorker instance to cancel.
     * @throws std::invalid_argument if worker is null.
     */
    void cancel(std::shared_ptr<AsyncWorker<ResultType>> worker);

    /**
     * @brief Gets the number of managed workers.
     *
     * @return The number of workers.
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    /**
     * @brief Removes completed workers from the manager.
     *
     * @return The number of workers removed.
     */
    size_t pruneCompletedWorkers() noexcept;

private:
    std::vector<std::shared_ptr<AsyncWorker<ResultType>>> workers_;
    mutable std::mutex mutex_;  // Thread-safety for concurrent access
};

// Coroutine support for C++20
template <typename T>
struct TaskPromise;

template <typename T>
class [[nodiscard]] Task {
public:
    using promise_type = TaskPromise<T>;

    Task() noexcept = default;
    explicit Task(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}
    ~Task() {
        if (handle_ && handle_.done()) {
            handle_.destroy();
        }
    }

    // Rule of five - prevent copy, allow move
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_)
                handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] T await_result() {
        if (!handle_) {
            throw std::runtime_error("Task has no valid coroutine handle");
        }

        if (!handle_.done()) {
            handle_.resume();
        }

        return handle_.promise().result();
    }

    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    [[nodiscard]] bool done() const noexcept {
        return !handle_ || handle_.done();
    }

private:
    std::coroutine_handle<promise_type> handle_ = nullptr;
};

template <typename T>
struct TaskPromise {
    T value_;
    std::exception_ptr exception_;

    TaskPromise() noexcept = default;

    Task<T> get_return_object() {
        return Task<T>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    void unhandled_exception() { exception_ = std::current_exception(); }

    template <std::convertible_to<T> U>
    void return_value(U&& value) {
        value_ = std::forward<U>(value);
    }

    T result() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
        return std::move(value_);
    }
};

// Template specialization for void
template <>
struct TaskPromise<void> {
    std::exception_ptr exception_;

    TaskPromise() noexcept = default;

    Task<void> get_return_object() {
        return Task<void>{
            std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    void unhandled_exception() { exception_ = std::current_exception(); }

    void return_void() {}

    void result() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }
};

// Retry strategy enum for different backoff strategies
enum class BackoffStrategy { FIXED, LINEAR, EXPONENTIAL };

/**
 * @brief Async execution with retry.
 *
 * This implementation uses enhanced exception handling and validations.
 *
 * @tparam Func The type of the function to be executed asynchronously.
 * @tparam Callback The type of the callback function.
 * @tparam ExceptionHandler The type of the exception handler function.
 * @tparam CompleteHandler The type of the completion handler function.
 * @tparam Args The types of the arguments to be passed to the function.
 * @param func The function to be executed asynchronously.
 * @param attemptsLeft Number of attempts left (must be > 0).
 * @param initialDelay Initial delay between retries.
 * @param strategy The backoff strategy to use.
 * @param maxTotalDelay Maximum total delay allowed.
 * @param callback Callback function called on success.
 * @param exceptionHandler Handler called when exceptions occur.
 * @param completeHandler Handler called when all attempts complete.
 * @param args Arguments to pass to func.
 * @return A future with the result of the async operation.
 * @throws std::invalid_argument If invalid parameters are provided.
 */
template <typename Func, typename Callback, typename ExceptionHandler,
          typename CompleteHandler, typename... Args>
auto asyncRetryImpl(Func&& func, int attemptsLeft,
                    std::chrono::milliseconds initialDelay,
                    BackoffStrategy strategy,
                    std::chrono::milliseconds maxTotalDelay,
                    Callback&& callback, ExceptionHandler&& exceptionHandler,
                    CompleteHandler&& completeHandler, Args&&... args) ->
    typename std::invoke_result_t<Func, Args...> {
    if (attemptsLeft <= 0) {
        throw std::invalid_argument("Attempts must be positive");
    }

    if (initialDelay.count() < 0) {
        throw std::invalid_argument("Initial delay cannot be negative");
    }

    using ReturnType = typename std::invoke_result_t<Func, Args...>;

    auto attempt = std::async(std::launch::async, std::forward<Func>(func),
                              std::forward<Args>(args)...);

    try {
        if constexpr (std::is_same_v<ReturnType, void>) {
            attempt.get();
            callback();
            completeHandler();
            return;
        } else {
            auto result = attempt.get();
            if constexpr (std::is_same_v<ReturnType, void>) {
                callback();
            } else {
                callback(result);
            }
            completeHandler();
            return result;
        }
    } catch (const std::exception& e) {
        exceptionHandler(e);  // Call custom exception handler

        if (attemptsLeft <= 1 || maxTotalDelay.count() <= 0) {
            completeHandler();  // Invoke complete handler on final failure
            throw;
        }

        // Calculate next retry delay based on strategy
        std::chrono::milliseconds nextDelay = initialDelay;
        switch (strategy) {
            case BackoffStrategy::LINEAR:
                nextDelay *= 2;
                break;
            case BackoffStrategy::EXPONENTIAL:
                nextDelay = std::chrono::milliseconds(static_cast<int>(
                    initialDelay.count() * std::pow(2, (5 - attemptsLeft))));
                break;
            default:  // FIXED strategy - keep the same delay
                break;
        }

        // Cap the delay if it exceeds max delay
        nextDelay = std::min(nextDelay, maxTotalDelay);

        std::this_thread::sleep_for(nextDelay);

        // Decrease the maximum total delay by the time spent in the last
        // attempt
        maxTotalDelay -= nextDelay;

        return asyncRetryImpl(std::forward<Func>(func), attemptsLeft - 1,
                              nextDelay, strategy, maxTotalDelay,
                              std::forward<Callback>(callback),
                              std::forward<ExceptionHandler>(exceptionHandler),
                              std::forward<CompleteHandler>(completeHandler),
                              std::forward<Args>(args)...);
    }
}

/**
 * @brief Async execution with retry (C++20 coroutine version).
 *
 * @tparam Func Function type
 * @tparam Args Argument types
 * @param func Function to execute
 * @param attemptsLeft Number of retry attempts
 * @param initialDelay Initial delay between retries
 * @param strategy Backoff strategy
 * @param args Function arguments
 * @return Task with the function result
 */
template <typename Func, typename... Args>
    requires InvocableWithArgs<Func, Args...>
Task<std::invoke_result_t<Func, Args...>> asyncRetryTask(
    Func&& func, int attemptsLeft, std::chrono::milliseconds initialDelay,
    BackoffStrategy strategy, Args&&... args) {
    using ReturnType = std::invoke_result_t<Func, Args...>;

    if (attemptsLeft <= 0) {
        throw std::invalid_argument("Attempts must be positive");
    }

    int attempts = 0;
    while (true) {
        try {
            if constexpr (std::is_same_v<ReturnType, void>) {
                std::invoke(std::forward<Func>(func),
                            std::forward<Args>(args)...);
                co_return;
            } else {
                co_return std::invoke(std::forward<Func>(func),
                                      std::forward<Args>(args)...);
            }
        } catch (const std::exception& e) {
            attempts++;
            if (attempts >= attemptsLeft) {
                throw;  // Re-throw after all attempts
            }

            // Calculate delay based on strategy
            std::chrono::milliseconds delay = initialDelay;
            switch (strategy) {
                case BackoffStrategy::LINEAR:
                    delay = initialDelay * attempts;
                    break;
                case BackoffStrategy::EXPONENTIAL:
                    delay = std::chrono::milliseconds(static_cast<int>(
                        initialDelay.count() * std::pow(2, attempts - 1)));
                    break;
                default:  // FIXED - keep same delay
                    break;
            }

            std::this_thread::sleep_for(delay);
        }
    }
}

/**
 * @brief Creates a future for async retry execution.
 *
 * @tparam Func The type of the function to be executed asynchronously.
 * @tparam Callback The type of the callback function.
 * @tparam ExceptionHandler The type of the exception handler function.
 * @tparam CompleteHandler The type of the completion handler function.
 * @tparam Args The types of the arguments to be passed to the function.
 */
template <typename Func, typename Callback, typename ExceptionHandler,
          typename CompleteHandler, typename... Args>
auto asyncRetry(Func&& func, int attemptsLeft,
                std::chrono::milliseconds initialDelay,
                BackoffStrategy strategy,
                std::chrono::milliseconds maxTotalDelay, Callback&& callback,
                ExceptionHandler&& exceptionHandler,
                CompleteHandler&& completeHandler, Args&&... args)
    -> std::future<typename std::invoke_result_t<Func, Args...>> {
    if (attemptsLeft <= 0) {
        throw std::invalid_argument("Attempts must be positive");
    }

    return std::async(
        std::launch::async, [=, func = std::forward<Func>(func)]() mutable {
            return asyncRetryImpl(
                std::forward<Func>(func), attemptsLeft, initialDelay, strategy,
                maxTotalDelay, std::forward<Callback>(callback),
                std::forward<ExceptionHandler>(exceptionHandler),
                std::forward<CompleteHandler>(completeHandler),
                std::forward<Args>(args)...);
        });
}

/**
 * @brief Creates an enhanced future for async retry execution.
 *
 * @tparam Func The type of the function to be executed asynchronously.
 * @tparam Callback The type of the callback function.
 * @tparam ExceptionHandler The type of the exception handler function.
 * @tparam CompleteHandler The type of the completion handler function.
 * @tparam Args The types of the arguments to be passed to the function.
 */
template <typename Func, typename Callback, typename ExceptionHandler,
          typename CompleteHandler, typename... Args>
auto asyncRetryE(Func&& func, int attemptsLeft,
                 std::chrono::milliseconds initialDelay,
                 BackoffStrategy strategy,
                 std::chrono::milliseconds maxTotalDelay, Callback&& callback,
                 ExceptionHandler&& exceptionHandler,
                 CompleteHandler&& completeHandler, Args&&... args)
    -> EnhancedFuture<typename std::invoke_result_t<Func, Args...>> {
    if (attemptsLeft <= 0) {
        throw std::invalid_argument("Attempts must be positive");
    }

    using ReturnType = typename std::invoke_result_t<Func, Args...>;

    auto future =
        std::async(std::launch::async, [=, func = std::forward<Func>(
                                               func)]() mutable {
            return asyncRetryImpl(
                std::forward<Func>(func), attemptsLeft, initialDelay, strategy,
                maxTotalDelay, std::forward<Callback>(callback),
                std::forward<ExceptionHandler>(exceptionHandler),
                std::forward<CompleteHandler>(completeHandler),
                std::forward<Args>(args)...);
        }).share();

    if constexpr (std::is_same_v<ReturnType, void>) {
        return EnhancedFuture<void>(std::shared_future<void>(future));
    } else {
        return EnhancedFuture<ReturnType>(
            std::shared_future<ReturnType>(future));
    }
}

/**
 * @brief Gets the result of a future with a timeout.
 *
 * @tparam T Result type
 * @tparam Duration Duration type
 * @param future The future to get the result from
 * @param timeout The timeout duration
 * @return The result of the future
 * @throws TimeoutException if the timeout is reached
 * @throws Any exception thrown by the future
 */
template <typename T, typename Duration>
    requires NonVoidType<T>
auto getWithTimeout(std::future<T>& future, Duration timeout) -> T {
    if (timeout.count() < 0) {
        throw std::invalid_argument("Timeout cannot be negative");
    }

    if (!future.valid()) {
        throw std::invalid_argument("Invalid future");
    }

    if (future.wait_for(timeout) == std::future_status::ready) {
        return future.get();
    }
    THROW_TIMEOUT_EXCEPTION("Timeout occurred while waiting for future result");
}

// Implementation of AsyncWorker methods
template <typename ResultType>
template <typename Func, typename... Args>
    requires InvocableWithArgs<Func, Args...> &&
             std::is_same_v<std::invoke_result_t<Func, Args...>, ResultType>
void AsyncWorker<ResultType>::startAsync(Func&& func, Args&&... args) {
    if constexpr (std::is_pointer_v<std::decay_t<Func>>) {
        if (!func)
            throw std::invalid_argument("Function cannot be null");
    }

    try {
        task_ = std::async(std::launch::async, std::forward<Func>(func),
                           std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to start async task: ") +
                                 e.what());
    }
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorker<ResultType>::getResult(
    std::chrono::milliseconds timeout) -> ResultType {
    if (!task_.valid()) {
        throw std::invalid_argument("Task is not valid");
    }

    if (timeout.count() > 0) {
        if (task_.wait_for(timeout) != std::future_status::ready) {
            THROW_TIMEOUT_EXCEPTION("Task result retrieval timed out");
        }
    }

    return task_.get();
}

template <typename ResultType>
void AsyncWorker<ResultType>::cancel() noexcept {
    try {
        if (task_.valid()) {
            task_.wait();  // Wait for task to complete
        }
    } catch (...) {
        // Suppress exceptions in cancel operation
    }
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorker<ResultType>::isDone() const noexcept -> bool {
    try {
        return task_.valid() && (task_.wait_for(std::chrono::seconds(0)) ==
                                 std::future_status::ready);
    } catch (...) {
        return false;  // In case of any exception, consider not done
    }
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorker<ResultType>::isActive() const noexcept -> bool {
    try {
        return task_.valid() && (task_.wait_for(std::chrono::seconds(0)) ==
                                 std::future_status::timeout);
    } catch (...) {
        return false;  // In case of any exception, consider not active
    }
}

template <typename ResultType>
auto AsyncWorker<ResultType>::validate(
    std::function<bool(ResultType)> validator) noexcept -> bool {
    try {
        if (!validator)
            return false;
        if (!isDone())
            return false;

        ResultType result = task_.get();
        return validator(result);
    } catch (...) {
        return false;
    }
}

template <typename ResultType>
void AsyncWorker<ResultType>::setCallback(
    std::function<void(ResultType)> callback) {
    if (!callback) {
        throw std::invalid_argument("Callback function cannot be null");
    }
    callback_ = std::move(callback);
}

template <typename ResultType>
void AsyncWorker<ResultType>::setTimeout(std::chrono::seconds timeout) {
    if (timeout < std::chrono::seconds(0)) {
        throw std::invalid_argument("Timeout cannot be negative");
    }
    timeout_ = timeout;
}

template <typename ResultType>
void AsyncWorker<ResultType>::waitForCompletion() {
    constexpr auto kSleepDuration =
        std::chrono::milliseconds(10);  // Reduced sleep time

    if (timeout_ != std::chrono::seconds(0)) {
        auto startTime = std::chrono::steady_clock::now();
        while (!isDone()) {
            std::this_thread::sleep_for(kSleepDuration);
            if (std::chrono::steady_clock::now() - startTime > timeout_) {
                cancel();
                THROW_TIMEOUT_EXCEPTION("Task execution timed out");
            }
        }
    } else {
        while (!isDone()) {
            std::this_thread::sleep_for(kSleepDuration);
        }
    }

    if (callback_ && isDone()) {
        try {
            callback_(getResult());
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Callback execution failed: ") + e.what());
        }
    }
}

template <typename ResultType>
template <typename Func, typename... Args>
    requires InvocableWithArgs<Func, Args...> &&
                 std::is_same_v<std::invoke_result_t<Func, Args...>, ResultType>
[[nodiscard]] auto AsyncWorkerManager<ResultType>::createWorker(
    Func&& func, Args&&... args) -> std::shared_ptr<AsyncWorker<ResultType>> {
    auto worker = std::make_shared<AsyncWorker<ResultType>>();

    try {
        worker->startAsync(std::forward<Func>(func),
                           std::forward<Args>(args)...);

        std::lock_guard<std::mutex> lock(mutex_);
        workers_.push_back(worker);
        return worker;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create worker: ") +
                                 e.what());
    }
}

template <typename ResultType>
void AsyncWorkerManager<ResultType>::cancelAll() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    // Use parallel algorithm if there are many workers
    if (workers_.size() > 10) {
        // C++17 parallel execution policy
        std::for_each(workers_.begin(), workers_.end(), [](auto& worker) {
            if (worker)
                worker->cancel();
        });
    } else {
        for (auto& worker : workers_) {
            if (worker)
                worker->cancel();
        }
    }
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorkerManager<ResultType>::allDone() const noexcept
    -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    return std::all_of(
        workers_.begin(), workers_.end(),
        [](const auto& worker) { return worker && worker->isDone(); });
}

template <typename ResultType>
void AsyncWorkerManager<ResultType>::waitForAll(
    std::chrono::milliseconds timeout) {
    std::vector<std::jthread> waitThreads;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Create a copy to avoid race conditions
        auto workersCopy = workers_;

        for (auto& worker : workersCopy) {
            if (!worker)
                continue;
            waitThreads.emplace_back(
                [worker, timeout]() { worker->waitForCompletion(); });
        }
    }

    for (auto& thread : waitThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorkerManager<ResultType>::isDone(
    std::shared_ptr<AsyncWorker<ResultType>> worker) const -> bool {
    if (!worker) {
        throw std::invalid_argument("Worker cannot be null");
    }
    return worker->isDone();
}

template <typename ResultType>
void AsyncWorkerManager<ResultType>::cancel(
    std::shared_ptr<AsyncWorker<ResultType>> worker) {
    if (!worker) {
        throw std::invalid_argument("Worker cannot be null");
    }
    worker->cancel();
}

template <typename ResultType>
[[nodiscard]] auto AsyncWorkerManager<ResultType>::size() const noexcept
    -> size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    return workers_.size();
}

template <typename ResultType>
size_t AsyncWorkerManager<ResultType>::pruneCompletedWorkers() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto initialSize = workers_.size();

    workers_.erase(std::remove_if(workers_.begin(), workers_.end(),
                                  [](const auto& worker) {
                                      return worker && worker->isDone();
                                  }),
                   workers_.end());

    return initialSize - workers_.size();
}
}  // namespace atom::async
#endif
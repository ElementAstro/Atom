#ifndef ATOM_ASYNC_PROMISE_HPP
#define ATOM_ASYNC_PROMISE_HPP

#include <atomic>
#include <concepts>
#include <coroutine>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "atom/async/future.hpp"

namespace atom::async {

/**
 * @class PromiseCancelledException
 * @brief Exception thrown when a promise is cancelled.
 */
class PromiseCancelledException : public atom::error::RuntimeError {
public:
    using atom::error::RuntimeError::RuntimeError;

    // Make the class more efficient with move semantics
    PromiseCancelledException(PromiseCancelledException&&) noexcept = default;
    PromiseCancelledException& operator=(PromiseCancelledException&&) noexcept =
        default;

    // 添加字符串构造函数
    explicit PromiseCancelledException(const char* message)
        : atom::error::RuntimeError(__FILE__, __LINE__, __func__, message) {}
};

/**
 * @def THROW_PROMISE_CANCELLED_EXCEPTION
 * @brief Macro to throw a PromiseCancelledException with file, line, and
 * function information.
 */
#define THROW_PROMISE_CANCELLED_EXCEPTION(...)                      \
    throw PromiseCancelledException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                    ATOM_FUNC_NAME, __VA_ARGS__);

/**
 * @def THROW_NESTED_PROMISE_CANCELLED_EXCEPTION
 * @brief Macro to rethrow a nested PromiseCancelledException with file, line,
 * and function information.
 */
#define THROW_NESTED_PROMISE_CANCELLED_EXCEPTION(...)   \
    PromiseCancelledException::rethrowNested(           \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
        "Promise cancelled: " __VA_ARGS__);

// Concept for valid callback function types
template <typename T, typename F>
concept CallbackInvocable = requires(F f, T value) {
    { f(value) } -> std::same_as<void>;
};

template <typename F>
concept VoidCallbackInvocable = requires(F f) {
    { f() } -> std::same_as<void>;
};

/**
 * @class Promise
 * @brief A template class that extends the standard promise with additional
 * features.
 * @tparam T The type of the value that the promise will hold.
 */
template <typename T>
class Promise {
public:
    /**
     * @brief Constructor that initializes the promise and shared future.
     */
    Promise() noexcept;

    // Rule of five for proper resource management
    ~Promise() noexcept = default;
    Promise(const Promise&) = delete;
    Promise& operator=(const Promise&) = delete;

    // 实现自定义的移动构造函数和移动赋值运算符，而不是默认
    Promise(Promise&& other) noexcept;
    Promise& operator=(Promise&& other) noexcept;

    /**
     * @brief Gets the enhanced future associated with this promise.
     * @return An EnhancedFuture object.
     */
    [[nodiscard]] auto getEnhancedFuture() noexcept -> EnhancedFuture<T>;

    /**
     * @brief Sets the value of the promise.
     * @param value The value to set.
     * @throws PromiseCancelledException if the promise has been cancelled.
     */
    template <typename U>
        requires std::convertible_to<U, T>
    void setValue(U&& value);

    /**
     * @brief Sets an exception for the promise.
     * @param exception The exception to set.
     * @throws PromiseCancelledException if the promise has been cancelled.
     */
    void setException(std::exception_ptr exception) noexcept(false);

    /**
     * @brief Adds a callback to be called when the promise is completed.
     * @tparam F The type of the callback function.
     * @param func The callback function to add.
     */
    template <typename F>
        requires CallbackInvocable<T, F>
    void onComplete(F&& func);

    /**
     * @brief Cancels the promise.
     * @return true if this call performed the cancellation, false if it was
     * already cancelled
     */
    [[nodiscard]] bool cancel() noexcept;

    /**
     * @brief Checks if the promise has been cancelled.
     * @return True if the promise has been cancelled, false otherwise.
     */
    [[nodiscard]] auto isCancelled() const noexcept -> bool;

    /**
     * @brief Gets the shared future associated with this promise.
     * @return A shared future object.
     */
    [[nodiscard]] auto getFuture() const noexcept -> std::shared_future<T>;

    /**
     * @brief Creates a coroutine awaiter for this promise.
     * @return A coroutine awaiter object.
     */
    [[nodiscard]] auto operator co_await() const noexcept;

private:
    /**
     * @brief Runs all the registered callbacks.
     * @throws Nothing. All exceptions from callbacks are caught and logged.
     */
    void runCallbacks() noexcept;

    std::promise<T> promise_;  ///< The underlying promise object.
    std::shared_future<T>
        future_;  ///< The shared future associated with the promise.

    // Use a mutex to protect callbacks for thread safety
    mutable std::shared_mutex mutex_;
    std::vector<std::function<void(T)>>
        callbacks_;  ///< List of callbacks to be called on completion.

    std::atomic<bool> cancelled_{
        false};  ///< Flag indicating if the promise has been cancelled.
    std::atomic<bool> completed_{
        false};  ///< Flag indicating if the promise has been completed.
};

/**
 * @class Promise<void>
 * @brief Specialization of the Promise class for void type.
 */
template <>
class Promise<void> {
public:
    /**
     * @brief Constructor that initializes the promise and shared future.
     */
    Promise() noexcept;

    // Rule of five for proper resource management
    ~Promise() noexcept = default;
    Promise(const Promise&) = delete;
    Promise& operator=(const Promise&) = delete;

    // 实现自定义的移动构造函数和移动赋值运算符，而不是默认
    Promise(Promise&& other) noexcept;
    Promise& operator=(Promise&& other) noexcept;

    /**
     * @brief Gets the enhanced future associated with this promise.
     * @return An EnhancedFuture object.
     */
    [[nodiscard]] auto getEnhancedFuture() noexcept -> EnhancedFuture<void>;

    /**
     * @brief Sets the value of the promise.
     * @throws PromiseCancelledException if the promise has been cancelled.
     */
    void setValue();

    /**
     * @brief Sets an exception for the promise.
     * @param exception The exception to set.
     * @throws PromiseCancelledException if the promise has been cancelled.
     */
    void setException(std::exception_ptr exception) noexcept(false);

    /**
     * @brief Adds a callback to be called when the promise is completed.
     * @tparam F The type of the callback function.
     * @param func The callback function to add.
     */
    template <typename F>
        requires VoidCallbackInvocable<F>
    void onComplete(F&& func);

    /**
     * @brief Cancels the promise.
     * @return true if this call performed the cancellation, false if it was
     * already cancelled
     */
    [[nodiscard]] bool cancel() noexcept;

    /**
     * @brief Checks if the promise has been cancelled.
     * @return True if the promise has been cancelled, false otherwise.
     */
    [[nodiscard]] auto isCancelled() const noexcept -> bool;

    /**
     * @brief Gets the shared future associated with this promise.
     * @return A shared future object.
     */
    [[nodiscard]] auto getFuture() const noexcept -> std::shared_future<void>;

    /**
     * @brief Creates a coroutine awaiter for this promise.
     * @return A coroutine awaiter object.
     */
    [[nodiscard]] auto operator co_await() const noexcept;

private:
    /**
     * @brief Runs all the registered callbacks.
     * @throws Nothing. All exceptions from callbacks are caught and logged.
     */
    void runCallbacks() noexcept;

    std::promise<void> promise_;  ///< The underlying promise object.
    std::shared_future<void>
        future_;  ///< The shared future associated with the promise.

    // Use a mutex to protect callbacks for thread safety
    mutable std::shared_mutex mutex_;
    std::vector<std::function<void()>>
        callbacks_;  ///< List of callbacks to be called on completion.

    std::atomic<bool> cancelled_{
        false};  ///< Flag indicating if the promise has been cancelled.
    std::atomic<bool> completed_{
        false};  ///< Flag indicating if the promise has been completed.
};

template <typename T>
Promise<T>::Promise() noexcept : future_(promise_.get_future().share()) {}

// 实现移动构造函数
template <typename T>
Promise<T>::Promise(Promise&& other) noexcept
    : promise_(std::move(other.promise_)), future_(std::move(other.future_)) {
    // 锁住 other 的互斥锁，确保安全移动
    std::unique_lock lock(other.mutex_);
    callbacks_ = std::move(other.callbacks_);
    cancelled_.store(other.cancelled_.load());
    completed_.store(other.completed_.load());
    // 移动后清除 other 的状态
    other.callbacks_.clear();
    other.cancelled_.store(false);
    other.completed_.store(false);
}

// 实现移动赋值运算符
template <typename T>
Promise<T>& Promise<T>::operator=(Promise&& other) noexcept {
    if (this != &other) {
        promise_ = std::move(other.promise_);
        future_ = std::move(other.future_);

        // 锁住双方互斥锁，确保安全移动
        std::scoped_lock lock(mutex_, other.mutex_);
        callbacks_ = std::move(other.callbacks_);
        cancelled_.store(other.cancelled_.load());
        completed_.store(other.completed_.load());

        // 移动后清除 other 的状态
        other.callbacks_.clear();
        other.cancelled_.store(false);
        other.completed_.store(false);
    }
    return *this;
}

template <typename T>
[[nodiscard]] auto Promise<T>::getEnhancedFuture() noexcept
    -> EnhancedFuture<T> {
    return EnhancedFuture<T>(future_);
}

template <typename T>
template <typename U>
    requires std::convertible_to<U, T>
void Promise<T>::setValue(U&& value) {
    if (isCancelled()) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set value, promise was cancelled.");
    }

    if (completed_.exchange(true)) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set value, promise was already completed.");
    }

    try {
        promise_.set_value(std::forward<U>(value));
        runCallbacks();  // Execute callbacks
    } catch (const std::exception& e) {
        // If we can't set the value due to a system exception, capture it
        try {
            promise_.set_exception(std::current_exception());
        } catch (...) {
            // Promise might already be satisfied or broken, ignore this
        }
        throw;  // Rethrow the original exception
    }
}

template <typename T>
void Promise<T>::setException(std::exception_ptr exception) noexcept(false) {
    if (isCancelled()) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set exception, promise was cancelled.");
    }

    if (completed_.exchange(true)) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set exception, promise was already completed.");
    }

    if (!exception) {
        exception = std::make_exception_ptr(std::invalid_argument(
            "Null exception pointer passed to setException"));
    }

    try {
        promise_.set_exception(exception);
        runCallbacks();  // Execute callbacks
    } catch (const std::exception&) {
        // Promise might already be satisfied or broken
        throw;  // Propagate the exception
    }
}

template <typename T>
template <typename F>
    requires CallbackInvocable<T, F>
void Promise<T>::onComplete(F&& func) {
    // First check if cancelled without acquiring the lock for better
    // performance
    if (isCancelled()) {
        return;  // No callbacks should be added if the promise is cancelled
    }

    bool shouldRunCallback = false;
    {
        std::unique_lock lock(mutex_);
        if (isCancelled()) {
            return;  // Double-check after acquiring the lock
        }

        // Store callback
        callbacks_.emplace_back(std::forward<F>(func));

        // Check if we should run the callback immediately
        shouldRunCallback =
            future_.valid() && future_.wait_for(std::chrono::seconds(0)) ==
                                   std::future_status::ready;
    }

    // Run callback outside the lock if needed
    if (shouldRunCallback) {
        try {
            T value = future_.get();
            func(value);
        } catch (...) {
            // Ignore exceptions from callback execution after the fact
        }
    }
}

template <typename T>
[[nodiscard]] bool Promise<T>::cancel() noexcept {
    bool expectedValue = false;
    const bool wasCancelled =
        cancelled_.compare_exchange_strong(expectedValue, true);

    if (wasCancelled) {
        // Only try to set exception if we were the ones who cancelled it
        try {
            // 修复：使用字符串构造 PromiseCancelledException
            promise_.set_exception(std::make_exception_ptr(
                PromiseCancelledException("Promise was explicitly cancelled")));
        } catch (...) {
            // Promise might already have a value or exception, ignore this
        }

        // Clear any pending callbacks
        std::unique_lock lock(mutex_);
        callbacks_.clear();
    }

    return wasCancelled;
}

template <typename T>
[[nodiscard]] auto Promise<T>::isCancelled() const noexcept -> bool {
    return cancelled_.load(std::memory_order_acquire);
}

template <typename T>
[[nodiscard]] auto Promise<T>::getFuture() const noexcept
    -> std::shared_future<T> {
    return future_;
}

template <typename T>
void Promise<T>::runCallbacks() noexcept {
    if (isCancelled()) {
        return;
    }

    // Make a local copy of callbacks to avoid holding the lock while executing
    // them
    std::vector<std::function<void(T)>> localCallbacks;
    {
        std::shared_lock lock(mutex_);
        if (callbacks_.empty())
            return;
        localCallbacks = std::move(callbacks_);
        callbacks_.clear();
    }

    if (future_.valid() && future_.wait_for(std::chrono::seconds(0)) ==
                               std::future_status::ready) {
        try {
            T value =
                future_.get();  // Get the value and pass it to the callbacks
            for (auto& callback : localCallbacks) {
                try {
                    callback(value);
                } catch (...) {
                    // Ignore exceptions from callbacks
                    // In a production system, you might want to log these
                }
            }
        } catch (...) {
            // Handle the case where the future contains an exception.
            // We don't invoke callbacks in this case.
        }
    }
}

template <typename T>
[[nodiscard]] auto Promise<T>::operator co_await() const noexcept {
    struct Awaiter {
        std::shared_future<T> future;

        bool await_ready() const noexcept {
            return future.wait_for(std::chrono::seconds(0)) ==
                   std::future_status::ready;
        }

        void await_suspend(std::coroutine_handle<> h) const {
            // Use std::thread to not block the coroutine
            std::thread([this, h]() mutable {
                future.wait();
                h.resume();
            }).detach();
        }

        T await_resume() const { return future.get(); }
    };

    return Awaiter{future_};
}

// Implementation for void specialization
Promise<void>::Promise() noexcept : future_(promise_.get_future().share()) {}

// 实现void特化的移动构造函数
Promise<void>::Promise(Promise&& other) noexcept
    : promise_(std::move(other.promise_)), future_(std::move(other.future_)) {
    std::unique_lock lock(other.mutex_);
    callbacks_ = std::move(other.callbacks_);
    cancelled_.store(other.cancelled_.load());
    completed_.store(other.completed_.load());
    other.callbacks_.clear();
    other.cancelled_.store(false);
    other.completed_.store(false);
}

// 实现void特化的移动赋值运算符
Promise<void>& Promise<void>::operator=(Promise&& other) noexcept {
    if (this != &other) {
        promise_ = std::move(other.promise_);
        future_ = std::move(other.future_);

        std::scoped_lock lock(mutex_, other.mutex_);
        callbacks_ = std::move(other.callbacks_);
        cancelled_.store(other.cancelled_.load());
        completed_.store(other.completed_.load());

        other.callbacks_.clear();
        other.cancelled_.store(false);
        other.completed_.store(false);
    }
    return *this;
}

[[nodiscard]] auto Promise<void>::getEnhancedFuture() noexcept
    -> EnhancedFuture<void> {
    return EnhancedFuture<void>(future_);
}

void Promise<void>::setValue() {
    if (isCancelled()) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set value, promise was cancelled.");
    }

    if (completed_.exchange(true)) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set value, promise was already completed.");
    }

    try {
        promise_.set_value();
        runCallbacks();  // Execute callbacks
    } catch (const std::exception& e) {
        // If we can't set the value due to a system exception, capture it
        try {
            promise_.set_exception(std::current_exception());
        } catch (...) {
            // Promise might already be satisfied or broken, ignore this
        }
        throw;  // Rethrow the original exception
    }
}

void Promise<void>::setException(std::exception_ptr exception) noexcept(false) {
    if (isCancelled()) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set exception, promise was cancelled.");
    }

    if (completed_.exchange(true)) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set exception, promise was already completed.");
    }

    if (!exception) {
        exception = std::make_exception_ptr(std::invalid_argument(
            "Null exception pointer passed to setException"));
    }

    try {
        promise_.set_exception(exception);
        runCallbacks();  // Execute callbacks
    } catch (const std::exception&) {
        // Promise might already be satisfied or broken
        throw;  // Propagate the exception
    }
}

template <typename F>
    requires VoidCallbackInvocable<F>
void Promise<void>::onComplete(F&& func) {
    // First check if cancelled without acquiring the lock for better
    // performance
    if (isCancelled()) {
        return;  // No callbacks should be added if the promise is cancelled
    }

    bool shouldRunCallback = false;
    {
        std::unique_lock lock(mutex_);
        if (isCancelled()) {
            return;  // Double-check after acquiring the lock
        }

        // Store callback
        callbacks_.emplace_back(std::forward<F>(func));

        // Check if we should run the callback immediately
        shouldRunCallback =
            future_.valid() && future_.wait_for(std::chrono::seconds(0)) ==
                                   std::future_status::ready;
    }

    // Run callback outside the lock if needed
    if (shouldRunCallback) {
        try {
            future_.get();
            func();
        } catch (...) {
            // Ignore exceptions from callback execution after the fact
        }
    }
}

[[nodiscard]] bool Promise<void>::cancel() noexcept {
    bool expectedValue = false;
    const bool wasCancelled =
        cancelled_.compare_exchange_strong(expectedValue, true);

    if (wasCancelled) {
        // Only try to set exception if we were the ones who cancelled it
        try {
            // 修复：使用字符串构造 PromiseCancelledException
            promise_.set_exception(std::make_exception_ptr(
                PromiseCancelledException("Promise was explicitly cancelled")));
        } catch (...) {
            // Promise might already have a value or exception, ignore this
        }

        // Clear any pending callbacks
        std::unique_lock lock(mutex_);
        callbacks_.clear();
    }

    return wasCancelled;
}

[[nodiscard]] auto Promise<void>::isCancelled() const noexcept -> bool {
    return cancelled_.load(std::memory_order_acquire);
}

[[nodiscard]] auto Promise<void>::getFuture() const noexcept
    -> std::shared_future<void> {
    return future_;
}

[[nodiscard]] auto Promise<void>::operator co_await() const noexcept {
    struct Awaiter {
        std::shared_future<void> future;

        bool await_ready() const noexcept {
            return future.wait_for(std::chrono::seconds(0)) ==
                   std::future_status::ready;
        }

        void await_suspend(std::coroutine_handle<> h) const {
            // Use std::thread to not block the coroutine
            std::thread([this, h]() mutable {
                future.wait();
                h.resume();
            }).detach();
        }

        void await_resume() const { future.get(); }
    };

    return Awaiter{future_};
}

void Promise<void>::runCallbacks() noexcept {
    if (isCancelled()) {
        return;
    }

    // Make a local copy of callbacks to avoid holding the lock while executing
    // them
    std::vector<std::function<void()>> localCallbacks;
    {
        std::shared_lock lock(mutex_);
        if (callbacks_.empty())
            return;
        localCallbacks = std::move(callbacks_);
        callbacks_.clear();
    }

    if (future_.valid() && future_.wait_for(std::chrono::seconds(0)) ==
                               std::future_status::ready) {
        try {
            future_.get();  // Check for exceptions
            for (auto& callback : localCallbacks) {
                try {
                    callback();
                } catch (...) {
                    // Ignore exceptions from callbacks
                    // In a production system, you might want to log these
                }
            }
        } catch (...) {
            // Handle the case where the future contains an exception.
            // We don't invoke callbacks in this case.
        }
    }
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_PROMISE_HPP

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
#include <source_location>
#include <stop_token>
#include <vector>

// Platform-specific optimizations
#if defined(_WIN32) || defined(_WIN64)
#define ATOM_PLATFORM_WINDOWS
#include <windows.h>
#elif defined(__APPLE__)
#define ATOM_PLATFORM_MACOS
#include <dispatch/dispatch.h>
#elif defined(__linux__)
#define ATOM_PLATFORM_LINUX
#include <pthread.h>
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#endif

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

    // Add string constructor, supporting C++20 source_location
    explicit PromiseCancelledException(
        const char* message,
        std::source_location location = std::source_location::current())
        : atom::error::RuntimeError(location.file_name(), location.line(),
                                    location.function_name(), message) {}
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

// New: Promise aware of C++20 coroutine state
template <typename T>
class PromiseAwaiter;

/**
 * @class Promise
 * @brief A template class that extends the standard promise with additional
 * features.
 * @tparam T The type of the value that the promise will hold.
 */
template <typename T>
class Promise {
public:
    // Support coroutines
    using awaiter_type = PromiseAwaiter<T>;

    /**
     * @brief Constructor that initializes the promise and shared future.
     */
    Promise() noexcept;

    // Rule of five for proper resource management
    ~Promise() noexcept = default;
    Promise(const Promise&) = delete;
    Promise& operator=(const Promise&) = delete;

    // Implement custom move constructor and move assignment operator instead of
    // default
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
     * @brief Use C++20 stop_token to support cancellable operations
     * @param stopToken The stop_token used to cancel the operation
     */
    void setCancellable(std::stop_token stopToken);

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

    /**
     * @brief Creates a PromiseAwaiter for this promise.
     * @return A PromiseAwaiter object.
     */
    [[nodiscard]] auto getAwaiter() noexcept -> PromiseAwaiter<T>;

    /**
     * @brief Perform asynchronous operations using platform-specific optimized
     * threads
     * @tparam F Function type
     * @tparam Args Argument types
     * @param func The function to execute
     * @param args Function arguments
     */
    template <typename F, typename... Args>
        requires std::invocable<F, Args...>
    void runAsync(F&& func, Args&&... args);

private:
    /**
     * @brief Runs all the registered callbacks.
     * @throws Nothing. All exceptions from callbacks are caught and logged.
     */
    void runCallbacks() noexcept;

    // Use C++20 jthread for thread management
    void setupCancellationHandler(std::stop_token token);

    std::promise<T> promise_;  ///< The underlying promise object.
    std::shared_future<T>
        future_;  ///< The shared future associated with the promise.

    // Use a mutex to protect callbacks for thread safety
    mutable std::shared_mutex mutex_;
#ifdef ATOM_USE_BOOST_LOCKFREE
    // Use lock-free queue to optimize callback performance
    struct CallbackWrapper {
        std::function<void(T)> callback;
        CallbackWrapper() = default;
        explicit CallbackWrapper(std::function<void(T)> cb)
            : callback(std::move(cb)) {}
    };

    boost::lockfree::queue<CallbackWrapper*> callbacks_{
        128};  ///< Lock-free callback queue
#else
    std::vector<std::function<void(T)>>
        callbacks_;  ///< List of callbacks to be called on completion.
#endif

    std::atomic<bool> cancelled_{
        false};  ///< Flag indicating if the promise has been cancelled.
    std::atomic<bool> completed_{
        false};  ///< Flag indicating if the promise has been completed.

    std::optional<std::jthread> cancellationThread_;
};

/**
 * @class Promise<void>
 * @brief Specialization of the Promise class for void type.
 */
template <>
class Promise<void> {
public:
    // Support coroutines
    using awaiter_type = PromiseAwaiter<void>;

    /**
     * @brief Constructor that initializes the promise and shared future.
     */
    Promise() noexcept;

    // Rule of five for proper resource management
    ~Promise() noexcept = default;
    Promise(const Promise&) = delete;
    Promise& operator=(const Promise&) = delete;

    // Implement custom move constructor and move assignment operator instead of
    // default
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
    void onComplete(F&& func) {
        // First check if cancelled without acquiring the lock for better
        // performance
        if (isCancelled()) {
            return;  // No callbacks should be added if the promise is cancelled
        }

        bool shouldRunCallback = false;
        {
#ifdef ATOM_USE_BOOST_LOCKFREE
            // Lock-free queue implementation
            auto* wrapper = new CallbackWrapper(std::forward<F>(func));
            callbacks_.push(wrapper);

            // Check if the callback should be run immediately
            shouldRunCallback =
                future_.valid() && future_.wait_for(std::chrono::seconds(0)) ==
                                       std::future_status::ready;
#else
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
#endif
        }

        // Run callback outside the lock if needed
        if (shouldRunCallback) {
            try {
                future_.get();  // Get the value (void)
#ifdef ATOM_USE_BOOST_LOCKFREE
                // For lock-free queue, we need to handle callback execution
                // manually
                CallbackWrapper* wrapper = nullptr;
                while (callbacks_.pop(wrapper)) {
                    if (wrapper && wrapper->callback) {
                        try {
                            wrapper->callback();
                        } catch (...) {
                            // Ignore exceptions in callbacks
                        }
                        delete wrapper;
                    }
                }
#else
                func();
#endif
            } catch (...) {
                // Ignore exceptions from callback execution after the fact
            }
        }
    }

    /**
     * @brief Use C++20 stop_token to support cancellable operations
     * @param stopToken The stop_token used to cancel the operation
     */
    void setCancellable(std::stop_token stopToken);

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

    /**
     * @brief Creates a PromiseAwaiter for this promise.
     * @return A PromiseAwaiter object.
     */
    [[nodiscard]] auto getAwaiter() noexcept -> PromiseAwaiter<void>;

    /**
     * @brief Perform asynchronous operations using platform-specific optimized
     * threads
     * @tparam F Function type
     * @tparam Args Argument types
     * @param func The function to execute
     * @param args Function arguments
     */
    template <typename F, typename... Args>
        requires std::invocable<F, Args...>
    void runAsync(F&& func, Args&&... args);

private:
    /**
     * @brief Runs all the registered callbacks.
     * @throws Nothing. All exceptions from callbacks are caught and logged.
     */
    void runCallbacks() noexcept;

    // Use C++20 jthread for thread management
    void setupCancellationHandler(std::stop_token token);

    std::promise<void> promise_;  ///< The underlying promise object.
    std::shared_future<void>
        future_;  ///< The shared future associated with the promise.

    // Use a mutex to protect callbacks for thread safety
    mutable std::shared_mutex mutex_;
#ifdef ATOM_USE_BOOST_LOCKFREE
    // Use lock-free queue to optimize callback performance
    struct CallbackWrapper {
        std::function<void()> callback;
        CallbackWrapper() = default;
        explicit CallbackWrapper(std::function<void()> cb)
            : callback(std::move(cb)) {}
    };

    boost::lockfree::queue<CallbackWrapper*> callbacks_{
        128};  ///< Lock-free callback queue
#else
    std::vector<std::function<void()>>
        callbacks_;  ///< List of callbacks to be called on completion.
#endif

    std::atomic<bool> cancelled_{
        false};  ///< Flag indicating if the promise has been cancelled.
    std::atomic<bool> completed_{
        false};  ///< Flag indicating if the promise has been completed.

    // C++20 jthread support
    std::optional<std::jthread> cancellationThread_;
};

// New: Coroutine awaiter implementation for Promise
template <typename T>
class PromiseAwaiter {
public:
    explicit PromiseAwaiter(std::shared_future<T> future) noexcept
        : future_(std::move(future)) {}

    bool await_ready() const noexcept {
        return future_.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready;
    }

    void await_suspend(std::coroutine_handle<> handle) const {
        // Platform-specific optimized implementation
#if defined(ATOM_PLATFORM_WINDOWS)
        // Windows optimized version
        auto thread = [](void* data) -> unsigned long {
            auto* params = static_cast<
                std::pair<std::shared_future<T>, std::coroutine_handle<>>*>(
                data);
            params->first.wait();
            params->second.resume();
            delete params;
            return 0;
        };

        auto* params =
            new std::pair<std::shared_future<T>, std::coroutine_handle<>>(
                future_, handle);
        HANDLE threadHandle =
            CreateThread(nullptr, 0, thread, params, 0, nullptr);
        if (threadHandle)
            CloseHandle(threadHandle);
#elif defined(ATOM_PLATFORM_MACOS)
        // macOS GCD optimized version
        auto* params =
            new std::pair<std::shared_future<T>, std::coroutine_handle<>>(
                future_, handle);
        dispatch_async_f(
            dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
            params, [](void* ctx) {
                auto* p = static_cast<
                    std::pair<std::shared_future<T>, std::coroutine_handle<>>*>(
                    ctx);
                p->first.wait();
                p->second.resume();
                delete p;
            });
#elif defined(ATOM_PLATFORM_LINUX)
        // Linux optimized version
        pthread_t thread;
        auto* params =
            new std::pair<std::shared_future<T>, std::coroutine_handle<>>(
                future_, handle);
        pthread_create(
            &thread, nullptr,
            [](void* data) -> void* {
                auto* p = static_cast<
                    std::pair<std::shared_future<T>, std::coroutine_handle<>>*>(
                    data);
                p->first.wait();
                p->second.resume();
                delete p;
                return nullptr;
            },
            params);
        pthread_detach(thread);
#else
        // Standard C++20 version
        std::jthread([future = future_, h = handle]() mutable {
            future.wait();
            h.resume();
        }).detach();
#endif
    }

    T await_resume() const { return future_.get(); }

private:
    std::shared_future<T> future_;
};

// void specialization
template <>
class PromiseAwaiter<void> {
public:
    explicit PromiseAwaiter(std::shared_future<void> future) noexcept
        : future_(std::move(future)) {}

    bool await_ready() const noexcept {
        return future_.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready;
    }

    void await_suspend(std::coroutine_handle<> handle) const {
        // Platform-specific implementation similar to non-void version, omitted
#if defined(ATOM_PLATFORM_WINDOWS)
        auto thread = [](void* data) -> unsigned long {
            auto* params = static_cast<
                std::pair<std::shared_future<void>, std::coroutine_handle<>>*>(
                data);
            params->first.wait();
            params->second.resume();
            delete params;
            return 0;
        };

        auto* params =
            new std::pair<std::shared_future<void>, std::coroutine_handle<>>(
                future_, handle);
        HANDLE threadHandle =
            CreateThread(nullptr, 0, thread, params, 0, nullptr);
        if (threadHandle)
            CloseHandle(threadHandle);
#elif defined(ATOM_PLATFORM_MACOS)
        auto* params =
            new std::pair<std::shared_future<void>, std::coroutine_handle<>>(
                future_, handle);
        dispatch_async_f(
            dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
            params, [](void* ctx) {
                auto* p = static_cast<std::pair<std::shared_future<void>,
                                                std::coroutine_handle<>>*>(ctx);
                p->first.wait();
                p->second.resume();
                delete p;
            });
#elif defined(ATOM_PLATFORM_LINUX)
        pthread_t thread;
        auto* params =
            new std::pair<std::shared_future<void>, std::coroutine_handle<>>(
                future_, handle);
        pthread_create(
            &thread, nullptr,
            [](void* data) -> void* {
                auto* p =
                    static_cast<std::pair<std::shared_future<void>,
                                          std::coroutine_handle<>>*>(data);
                p->first.wait();
                p->second.resume();
                delete p;
                return nullptr;
            },
            params);
        pthread_detach(thread);
#else
        std::jthread([future = future_, h = handle]() mutable {
            future.wait();
            h.resume();
        }).detach();
#endif
    }

    void await_resume() const { future_.get(); }

private:
    std::shared_future<void> future_;
};

template <typename T>
Promise<T>::Promise() noexcept : future_(promise_.get_future().share()) {}

// Implement move constructor
template <typename T>
Promise<T>::Promise(Promise&& other) noexcept
    : promise_(std::move(other.promise_)), future_(std::move(other.future_)) {
    // Lock other's mutex to ensure safe move
#ifdef ATOM_USE_BOOST_LOCKFREE
    // Special handling for lock-free queue
    // Lock-free queue cannot be moved directly, need to transfer elements one
    // by one
    CallbackWrapper* wrapper = nullptr;
    while (other.callbacks_.pop(wrapper)) {
        if (wrapper) {
            callbacks_.push(wrapper);
        }
    }
#else
    std::unique_lock lock(other.mutex_);
    callbacks_ = std::move(other.callbacks_);
#endif
    cancelled_.store(other.cancelled_.load());
    completed_.store(other.completed_.load());

    // Handle cancellation thread
    if (other.cancellationThread_.has_value()) {
        cancellationThread_ = std::move(other.cancellationThread_);
        other.cancellationThread_.reset();
    }

    // Clear other's state after move
#ifndef ATOM_USE_BOOST_LOCKFREE
    other.callbacks_.clear();
#endif
    other.cancelled_.store(false);
    other.completed_.store(false);
}

// Implement move assignment operator
template <typename T>
Promise<T>& Promise<T>::operator=(Promise&& other) noexcept {
    if (this != &other) {
        promise_ = std::move(other.promise_);
        future_ = std::move(other.future_);

#ifdef ATOM_USE_BOOST_LOCKFREE
        // Clean up current queue
        CallbackWrapper* wrapper = nullptr;
        while (callbacks_.pop(wrapper)) {
            delete wrapper;
        }

        // Transfer elements
        while (other.callbacks_.pop(wrapper)) {
            if (wrapper) {
                callbacks_.push(wrapper);
            }
        }
#else
        // Lock both mutexes to ensure safe move
        std::scoped_lock lock(mutex_, other.mutex_);
        callbacks_ = std::move(other.callbacks_);
#endif
        cancelled_.store(other.cancelled_.load());
        completed_.store(other.completed_.load());

        // Handle cancellation thread
        if (cancellationThread_.has_value()) {
            cancellationThread_->request_stop();
        }
        if (other.cancellationThread_.has_value()) {
            cancellationThread_ = std::move(other.cancellationThread_);
            other.cancellationThread_.reset();
        }

        // Clear other's state after move
#ifndef ATOM_USE_BOOST_LOCKFREE
        other.callbacks_.clear();
#endif
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
#ifdef ATOM_USE_BOOST_LOCKFREE
        // Lock-free queue implementation
        auto* wrapper = new CallbackWrapper(std::forward<F>(func));
        callbacks_.push(wrapper);

        // Check if the callback should be run immediately
        shouldRunCallback =
            future_.valid() && future_.wait_for(std::chrono::seconds(0)) ==
                                   std::future_status::ready;
#else
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
#endif
    }

    // Run callback outside the lock if needed
    if (shouldRunCallback) {
        try {
            T value = future_.get();
#ifdef ATOM_USE_BOOST_LOCKFREE
            // For lock-free queue, we need to handle callback execution
            // manually
            CallbackWrapper* wrapper = nullptr;
            while (callbacks_.pop(wrapper)) {
                if (wrapper && wrapper->callback) {
                    try {
                        wrapper->callback(value);
                    } catch (...) {
                        // Ignore exceptions in callbacks
                    }
                    delete wrapper;
                }
            }
#else
            func(value);
#endif
        } catch (...) {
            // Ignore exceptions from callback execution after the fact
        }
    }
}

template <typename T>
void Promise<T>::setCancellable(std::stop_token stopToken) {
    if (stopToken.stop_possible()) {
        setupCancellationHandler(stopToken);
    }
}

template <typename T>
void Promise<T>::setupCancellationHandler(std::stop_token token) {
    // Use jthread to automatically manage the cancellation handler
    cancellationThread_.emplace([this, token](std::stop_token localToken) {
        std::stop_callback callback(token, [this]() { cancel(); });

        // Wait until the local token is stopped or the promise is completed
        while (!localToken.stop_requested() && !completed_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

template <typename T>
[[nodiscard]] bool Promise<T>::cancel() noexcept {
    bool expectedValue = false;
    const bool wasCancelled =
        cancelled_.compare_exchange_strong(expectedValue, true);

    if (wasCancelled) {
        // Only try to set exception if we were the ones who cancelled it
        try {
            // Fix: Use string to construct PromiseCancelledException
            promise_.set_exception(std::make_exception_ptr(
                PromiseCancelledException("Promise was explicitly cancelled")));
        } catch (...) {
            // Promise might already have a value or exception, ignore this
        }

        // Clear any pending callbacks
#ifdef ATOM_USE_BOOST_LOCKFREE
        // Clean up lock-free queue
        CallbackWrapper* wrapper = nullptr;
        while (callbacks_.pop(wrapper)) {
            delete wrapper;
        }
#else
        std::unique_lock lock(mutex_);
        callbacks_.clear();
#endif
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

#ifdef ATOM_USE_BOOST_LOCKFREE
    // Lock-free queue version
    if (callbacks_.empty())
        return;

    if (future_.valid() && future_.wait_for(std::chrono::seconds(0)) ==
                               std::future_status::ready) {
        try {
            T value = future_.get();  // Get the value
            CallbackWrapper* wrapper = nullptr;
            while (callbacks_.pop(wrapper)) {
                if (wrapper && wrapper->callback) {
                    try {
                        wrapper->callback(value);
                    } catch (...) {
                        // Ignore exceptions in callbacks
                    }
                    delete wrapper;
                }
            }
        } catch (...) {
            // Handle the case where the future contains an exception
            // Clean up callbacks but do not execute
            CallbackWrapper* wrapper = nullptr;
            while (callbacks_.pop(wrapper)) {
                delete wrapper;
            }
        }
    }
#else
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
#endif
}

template <typename T>
[[nodiscard]] auto Promise<T>::operator co_await() const noexcept {
    return PromiseAwaiter<T>(future_);
}

template <typename T>
[[nodiscard]] auto Promise<T>::getAwaiter() noexcept -> PromiseAwaiter<T> {
    return PromiseAwaiter<T>(future_);
}

template <typename T>
template <typename F, typename... Args>
    requires std::invocable<F, Args...>
void Promise<T>::runAsync(F&& func, Args&&... args) {
    if (isCancelled()) {
        return;
    }

    // Use platform-specific thread optimization for asynchronous execution
#if defined(ATOM_PLATFORM_WINDOWS)
    // Windows thread pool optimization
    struct ThreadData {
        Promise<T>* promise;
        std::tuple<std::decay_t<F>, std::decay_t<Args>...> func_and_args;

        ThreadData(Promise<T>* p, F&& f, Args&&... a)
            : promise(p),
              func_and_args(std::forward<F>(f), std::forward<Args>(a)...) {}

        static unsigned long WINAPI ThreadProc(void* param) {
            auto* data = static_cast<ThreadData*>(param);
            try {
                if constexpr (std::is_void_v<
                                  std::invoke_result_t<F, Args...>>) {
                    // Handle void return function
                    std::apply(
                        [](auto&&... args) {
                            std::invoke(std::forward<decltype(args)>(args)...);
                        },
                        data->func_and_args);

                    // For void return type functions, need special handling for
                    // Promise<T> type
                    if constexpr (std::is_void_v<T>) {
                        data->promise->setValue();
                    } else {
                        // This case is actually a type mismatch, should cause
                        // compile error Handle runtime case here only
                    }
                } else {
                    // Handle function with return value
                    auto result = std::apply(
                        [](auto&&... args) {
                            return std::invoke(
                                std::forward<decltype(args)>(args)...);
                        },
                        data->func_and_args);

                    if constexpr (std::is_convertible_v<
                                      std::invoke_result_t<F, Args...>, T>) {
                        data->promise->setValue(std::move(result));
                    }
                }
            } catch (...) {
                data->promise->setException(std::current_exception());
            }
            delete data;
            return 0;
        }
    };

    auto* threadData = new ThreadData(this, std::forward<F>(func),
                                      std::forward<Args>(args)...);
    HANDLE threadHandle = CreateThread(nullptr, 0, ThreadData::ThreadProc,
                                       threadData, 0, nullptr);
    if (threadHandle) {
        CloseHandle(threadHandle);
    } else {
        // Failed to create thread, clean up resources
        delete threadData;
        setException(std::make_exception_ptr(
            std::runtime_error("Failed to create thread")));
    }
#elif defined(ATOM_PLATFORM_MACOS)
    // macOS GCD optimization
    struct DispatchData {
        Promise<T>* promise;
        std::tuple<std::decay_t<F>, std::decay_t<Args>...> func_and_args;

        DispatchData(Promise<T>* p, F&& f, Args&&... a)
            : promise(p),
              func_and_args(std::forward<F>(f), std::forward<Args>(a)...) {}

        static void Execute(void* context) {
            auto* data = static_cast<DispatchData*>(context);
            try {
                if constexpr (std::is_void_v<
                                  std::invoke_result_t<F, Args...>>) {
                    std::apply(
                        [](auto&&... args) {
                            std::invoke(std::forward<decltype(args)>(args)...);
                        },
                        data->func_and_args);

                    if constexpr (std::is_void_v<T>) {
                        data->promise->setValue();
                    }
                } else {
                    auto result = std::apply(
                        [](auto&&... args) {
                            return std::invoke(
                                std::forward<decltype(args)>(args)...);
                        },
                        data->func_and_args);

                    if constexpr (std::is_convertible_v<
                                      std::invoke_result_t<F, Args...>, T>) {
                        data->promise->setValue(std::move(result));
                    }
                }
            } catch (...) {
                data->promise->setException(std::current_exception());
            }
            delete data;
        }
    };

    auto* dispatchData = new DispatchData(this, std::forward<F>(func),
                                          std::forward<Args>(args)...);
    dispatch_async_f(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        dispatchData, DispatchData::Execute);
#else
    // Standard C++20 implementation
    std::jthread([this, func = std::forward<F>(func),
                  ... args = std::forward<Args>(args)]() mutable {
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<F, Args...>>) {
                std::invoke(func, args...);

                if constexpr (std::is_void_v<T>) {
                    this->setValue();
                }
            } else {
                auto result = std::invoke(func, args...);

                if constexpr (std::is_convertible_v<
                                  std::invoke_result_t<F, Args...>, T>) {
                    this->setValue(std::move(result));
                }
            }
        } catch (...) {
            this->setException(std::current_exception());
        }
    }).detach();
#endif
}

template <typename F, typename... Args>
    requires std::invocable<F, Args...>
void Promise<void>::runAsync(F&& func, Args&&... args) {
    if (isCancelled()) {
        return;
    }

    // Use platform-specific thread optimization for asynchronous execution,
    // similar to non-void version
#if defined(ATOM_PLATFORM_WINDOWS)
    struct ThreadData {
        Promise<void>* promise;
        std::tuple<std::decay_t<F>, std::decay_t<Args>...> func_and_args;

        ThreadData(Promise<void>* p, F&& f, Args&&... a)
            : promise(p),
              func_and_args(std::forward<F>(f), std::forward<Args>(a)...) {}

        static unsigned long WINAPI ThreadProc(void* param) {
            auto* data = static_cast<ThreadData*>(param);
            try {
                std::apply(
                    [](auto&&... args) {
                        std::invoke(std::forward<decltype(args)>(args)...);
                    },
                    data->func_and_args);
                data->promise->setValue();
            } catch (...) {
                data->promise->setException(std::current_exception());
            }
            delete data;
            return 0;
        }
    };

    auto* threadData = new ThreadData(this, std::forward<F>(func),
                                      std::forward<Args>(args)...);
    HANDLE threadHandle = CreateThread(nullptr, 0, ThreadData::ThreadProc,
                                       threadData, 0, nullptr);
    if (threadHandle) {
        CloseHandle(threadHandle);
    } else {
        delete threadData;
        setException(std::make_exception_ptr(
            std::runtime_error("Failed to create thread")));
    }
#elif defined(ATOM_PLATFORM_MACOS)
    struct DispatchData {
        Promise<void>* promise;
        std::tuple<std::decay_t<F>, std::decay_t<Args>...> func_and_args;

        DispatchData(Promise<void>* p, F&& f, Args&&... a)
            : promise(p),
              func_and_args(std::forward<F>(f), std::forward<Args>(a)...) {}

        static void Execute(void* context) {
            auto* data = static_cast<DispatchData*>(context);
            try {
                std::apply(
                    [](auto&&... args) {
                        std::invoke(std::forward<decltype(args)>(args)...);
                    },
                    data->func_and_args);
                data->promise->setValue();
            } catch (...) {
                data->promise->setException(std::current_exception());
            }
            delete data;
        }
    };

    auto* dispatchData = new DispatchData(this, std::forward<F>(func),
                                          std::forward<Args>(args)...);
    dispatch_async_f(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        dispatchData, DispatchData::Execute);
#else
    std::jthread([this, func = std::forward<F>(func),
                  ... args = std::forward<Args>(args)]() mutable {
        try {
            std::invoke(func, args...);
            this->setValue();
        } catch (...) {
            this->setException(std::current_exception());
        }
    }).detach();
#endif
}

// New: Helper function to create a completed Promise
template <typename T>
auto makeReadyPromise(T value) {
    Promise<T> promise;
    promise.setValue(std::move(value));
    return promise;
}

// void specialization
inline auto makeReadyPromise() {
    Promise<void> promise;
    promise.setValue();
    return promise;
}

// New: Create a cancelled Promise
template <typename T>
auto makeCancelledPromise() {
    Promise<T> promise;
    promise.cancel();
    return promise;
}

// New: Create an asynchronously executed Promise from a function
template <typename F, typename... Args>
    requires std::invocable<F, Args...>
auto makePromiseFromFunction(F&& func, Args&&... args) {
    using ResultType = std::invoke_result_t<F, Args...>;

    if constexpr (std::is_void_v<ResultType>) {
        Promise<void> promise;
        promise.runAsync(std::forward<F>(func), std::forward<Args>(args)...);
        return promise;
    } else {
        Promise<ResultType> promise;
        promise.runAsync(std::forward<F>(func), std::forward<Args>(args)...);
        return promise;
    }
}

// New: Combine multiple Promises, return result array when all Promises
// complete
template <typename T>
auto whenAll(std::vector<Promise<T>>& promises) {
    Promise<std::vector<T>> resultPromise;

    if (promises.empty()) {
        resultPromise.setValue(std::vector<T>{});
        return resultPromise;
    }

    // Create shared state to track completion status
    struct SharedState {
        std::mutex mutex;
        std::vector<T> results;
        size_t completedCount = 0;
        size_t totalCount;
        Promise<std::vector<T>> resultPromise;
        std::vector<std::exception_ptr> exceptions;

        explicit SharedState(size_t count, Promise<std::vector<T>> promise)
            : totalCount(count), resultPromise(std::move(promise)) {
            results.resize(count);
        }
    };

    auto state = std::make_shared<SharedState>(promises.size(), resultPromise);

    // Set callback for each promise
    for (size_t i = 0; i < promises.size(); ++i) {
        promises[i].onComplete([state, i](T value) {
            std::unique_lock lock(state->mutex);
            state->results[i] = std::move(value);
            state->completedCount++;

            if (state->completedCount == state->totalCount) {
                if (state->exceptions.empty()) {
                    state->resultPromise.setValue(std::move(state->results));
                } else {
                    // If there are any exceptions, propagate the first one to
                    // the result Promise
                    state->resultPromise.setException(state->exceptions[0]);
                }
            }
        });
    }

    return resultPromise;
}

// void specialization
inline auto whenAll(std::vector<Promise<void>>& promises) {
    Promise<void> resultPromise;

    if (promises.empty()) {
        resultPromise.setValue();
        return resultPromise;
    }

    // Create shared state to track completion status
    struct SharedState {
        std::mutex mutex;
        size_t completedCount = 0;
        size_t totalCount;
        Promise<void> resultPromise;
        std::vector<std::exception_ptr> exceptions;

        explicit SharedState(size_t count, Promise<void> promise)
            : totalCount(count), resultPromise(std::move(promise)) {}
    };

    auto state = std::make_shared<SharedState>(promises.size(), resultPromise);

    // Set callback for each promise
    for (size_t i = 0; i < promises.size(); ++i) {
        promises[i].onComplete([state]() {
            std::unique_lock lock(state->mutex);
            state->completedCount++;

            if (state->completedCount == state->totalCount) {
                if (state->exceptions.empty()) {
                    state->resultPromise.setValue();
                } else {
                    // If there are any exceptions, propagate the first one to
                    // the result Promise
                    state->resultPromise.setException(state->exceptions[0]);
                }
            }
        });
    }

    return resultPromise;
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_PROMISE_HPP

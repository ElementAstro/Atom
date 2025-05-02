#ifndef ATOM_ASYNC_FUTURE_HPP
#define ATOM_ASYNC_FUTURE_HPP

#include <atomic>
#include <concepts>
#include <coroutine>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <ranges>
#include <thread>
#include <type_traits>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#define ATOM_PLATFORM_WINDOWS
#include <windows.h>
#elif defined(__APPLE__)
#define ATOM_PLATFORM_MACOS
#include <dispatch/dispatch.h>
#elif defined(__linux__)
#define ATOM_PLATFORM_LINUX
#include <sys/sysinfo.h>
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#endif

#include "atom/error/exception.hpp"

namespace atom::async {

/**
 * @class InvalidFutureException
 * @brief Exception thrown when an invalid future is encountered.
 */
class InvalidFutureException : public atom::error::RuntimeError {
public:
    using atom::error::RuntimeError::RuntimeError;
};

/**
 * @def THROW_INVALID_FUTURE_EXCEPTION
 * @brief Macro to throw an InvalidFutureException with file, line, and function
 * information.
 */
#define THROW_INVALID_FUTURE_EXCEPTION(...)                      \
    throw InvalidFutureException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                 ATOM_FUNC_NAME, __VA_ARGS__);

/**
 * @def THROW_NESTED_INVALID_FUTURE_EXCEPTION
 * @brief Macro to rethrow a nested InvalidFutureException with file, line, and
 * function information.
 */
#define THROW_NESTED_INVALID_FUTURE_EXCEPTION(...)                        \
    InvalidFutureException::rethrowNested(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                          ATOM_FUNC_NAME,                 \
                                          "Invalid future: " __VA_ARGS__);

// Concept to ensure a type can be used in a future
template <typename T>
concept FutureCompatible = std::is_object_v<T> || std::is_void_v<T>;

// Concept to ensure a callable can be used with specific arguments
template <typename F, typename... Args>
concept ValidCallable = requires(F&& f, Args&&... args) {
    { std::invoke(std::forward<F>(f), std::forward<Args>(args)...) };
};

// 新增：协程等待对象辅助类
template <typename T>
class [[nodiscard]] AwaitableEnhancedFuture {
public:
    explicit AwaitableEnhancedFuture(std::shared_future<T> future)
        : future_(std::move(future)) {}

    bool await_ready() const noexcept {
        return future_.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready;
    }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) const {
#if defined(ATOM_PLATFORM_WINDOWS)
        // Windows 线程池优化
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
#else
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
template <>
class [[nodiscard]] AwaitableEnhancedFuture<void> {
public:
    explicit AwaitableEnhancedFuture(std::shared_future<void> future)
        : future_(std::move(future)) {}

    bool await_ready() const noexcept {
        return future_.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready;
    }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) const {
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

/**
 * @class EnhancedFuture
 * @brief A template class that extends the standard future with additional
 * features, enhanced with C++20 features.
 * @tparam T The type of the value that the future will hold.
 */
template <FutureCompatible T>
class EnhancedFuture {
public:
    // Enable coroutine support
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

#ifdef ATOM_USE_BOOST_LOCKFREE
    /**
     * @brief Callback wrapper for lockfree queue
     */
    struct CallbackWrapper {
        std::function<void(T)> callback;

        CallbackWrapper() = default;
        explicit CallbackWrapper(std::function<void(T)> cb)
            : callback(std::move(cb)) {}
    };

    /**
     * @brief Lockfree callback container
     */
    class LockfreeCallbackContainer {
    public:
        LockfreeCallbackContainer() : queue_(128) {}  // Default capacity

        void add(const std::function<void(T)>& callback) {
            auto* wrapper = new CallbackWrapper(callback);
            // Try pushing until successful
            while (!queue_.push(wrapper)) {
                std::this_thread::yield();
            }
        }

        void executeAll(const T& value) {
            CallbackWrapper* wrapper = nullptr;
            while (queue_.pop(wrapper)) {
                if (wrapper && wrapper->callback) {
                    try {
                        wrapper->callback(value);
                    } catch (...) {
                        // Log error but continue with other callbacks
                    }
                    delete wrapper;
                }
            }
        }

        bool empty() const { return queue_.empty(); }

        ~LockfreeCallbackContainer() {
            CallbackWrapper* wrapper = nullptr;
            while (queue_.pop(wrapper)) {
                delete wrapper;
            }
        }

    private:
        boost::lockfree::queue<CallbackWrapper*> queue_;
    };
#endif

    /**
     * @brief Constructs an EnhancedFuture from a shared future.
     * @param fut The shared future to wrap.
     */
    explicit EnhancedFuture(std::shared_future<T>&& fut) noexcept
        : future_(std::move(fut)),
          cancelled_(std::make_shared<std::atomic<bool>>(false))
#ifdef ATOM_USE_BOOST_LOCKFREE
          ,
          callbacks_(std::make_shared<LockfreeCallbackContainer>())
#else
          ,
          callbacks_(std::make_shared<std::vector<std::function<void(T)>>>())
#endif
    {
    }

    explicit EnhancedFuture(const std::shared_future<T>& fut) noexcept
        : future_(fut),
          cancelled_(std::make_shared<std::atomic<bool>>(false))
#ifdef ATOM_USE_BOOST_LOCKFREE
          ,
          callbacks_(std::make_shared<LockfreeCallbackContainer>())
#else
          ,
          callbacks_(std::make_shared<std::vector<std::function<void(T)>>>())
#endif
    {
    }

    // Move constructor and assignment
    EnhancedFuture(EnhancedFuture&& other) noexcept = default;
    EnhancedFuture& operator=(EnhancedFuture&& other) noexcept = default;

    // Copy constructor and assignment
    EnhancedFuture(const EnhancedFuture&) = default;
    EnhancedFuture& operator=(const EnhancedFuture&) = default;

    /**
     * @brief Chains another operation to be called after the future is done.
     * @tparam F The type of the function to call.
     * @param func The function to call when the future is done.
     * @return An EnhancedFuture for the result of the function.
     */
    template <ValidCallable<T> F>
    auto then(F&& func) {
        using ResultType = std::invoke_result_t<F, T>;
        auto sharedFuture = std::make_shared<std::shared_future<T>>(future_);
        auto sharedCancelled = cancelled_;  // Share the cancelled flag

        return EnhancedFuture<ResultType>(
            std::async(std::launch::async,
                       [sharedFuture, sharedCancelled,
                        func = std::forward<F>(func)]() -> ResultType {
                           if (*sharedCancelled) {
                               THROW_INVALID_FUTURE_EXCEPTION(
                                   "Future has been cancelled");
                           }

                           if (sharedFuture->valid()) {
                               try {
                                   return func(sharedFuture->get());
                               } catch (...) {
                                   THROW_NESTED_INVALID_FUTURE_EXCEPTION(
                                       "Exception in then callback");
                               }
                           }
                           THROW_INVALID_FUTURE_EXCEPTION("Future is invalid");
                       })
                .share());
    }

    /**
     * @brief Waits for the future with a timeout and auto-cancels if not ready.
     * @param timeout The timeout duration.
     * @return An optional containing the value if ready, or nullopt if timed
     * out.
     */
    auto waitFor(std::chrono::milliseconds timeout) noexcept
        -> std::optional<T> {
        if (future_.wait_for(timeout) == std::future_status::ready &&
            !*cancelled_) {
            try {
                return future_.get();
            } catch (...) {
                return std::nullopt;
            }
        }
        cancel();
        return std::nullopt;
    }

    /**
     * @brief Enhanced timeout wait with custom cancellation policy
     * @param timeout The timeout duration
     * @param cancelPolicy The cancellation policy function
     * @return Optional value, empty if timed out
     */
    template <typename Rep, typename Period,
              typename CancelFunc = std::function<void()>>
    auto waitFor(
        std::chrono::duration<Rep, Period> timeout,
        CancelFunc&& cancelPolicy = []() {}) noexcept -> std::optional<T> {
        if (future_.wait_for(timeout) == std::future_status::ready &&
            !*cancelled_) {
            try {
                return future_.get();
            } catch (...) {
                return std::nullopt;
            }
        }

        cancel();
        if constexpr (!std::is_same_v<CancelFunc, std::function<void()>>) {
            std::invoke(std::forward<CancelFunc>(cancelPolicy));
        }
        return std::nullopt;
    }

    /**
     * @brief Checks if the future is done.
     * @return True if the future is done, false otherwise.
     */
    [[nodiscard]] auto isDone() const noexcept -> bool {
        return future_.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    }

    /**
     * @brief Sets a completion callback to be called when the future is done.
     * @tparam F The type of the callback function.
     * @param func The callback function to add.
     */
    template <ValidCallable<T> F>
    void onComplete(F&& func) {
        if (*cancelled_) {
            return;
        }

#ifdef ATOM_USE_BOOST_LOCKFREE
        callbacks_->add(std::forward<F>(func));

        std::thread([future = future_, callbacks = callbacks_,
                     cancelled = cancelled_]() mutable {
            try {
                if (!*cancelled && future.valid()) {
                    T result = future.get();
                    if (!*cancelled) {
                        callbacks->executeAll(result);
                    }
                }
            } catch (...) {
                // Future completed with exception
            }
        }).detach();
#else
        callbacks_->emplace_back(std::forward<F>(func));

        std::thread([future = future_, callbacks = callbacks_,
                     cancelled = cancelled_]() mutable {
            try {
                if (!*cancelled && future.valid()) {
                    T result = future.get();
                    for (auto& callback : *callbacks) {
                        try {
                            callback(result);
                        } catch (...) {
                            // Log error but continue with other callbacks
                        }
                    }
                }
            } catch (...) {
                // Future completed with exception
            }
        }).detach();
#endif
    }

    /**
     * @brief Waits synchronously for the future to complete.
     * @return The value of the future.
     * @throws InvalidFutureException if the future is cancelled.
     */
    auto wait() -> T {
        if (*cancelled_) {
            THROW_INVALID_FUTURE_EXCEPTION("Future has been cancelled");
        }

        try {
            return future_.get();
        } catch (const std::exception& e) {
            THROW_NESTED_INVALID_FUTURE_EXCEPTION(
                "Exception while waiting for future: ", e.what());
        } catch (...) {
            THROW_NESTED_INVALID_FUTURE_EXCEPTION(
                "Unknown exception while waiting for future");
        }
    }

    template <ValidCallable<std::exception_ptr> F>
    auto catching(F&& func) {
        using ResultType = T;
        auto sharedFuture = std::make_shared<std::shared_future<T>>(future_);
        auto sharedCancelled = cancelled_;

        return EnhancedFuture<ResultType>(
            std::async(std::launch::async,
                       [sharedFuture, sharedCancelled,
                        func = std::forward<F>(func)]() -> ResultType {
                           if (*sharedCancelled) {
                               THROW_INVALID_FUTURE_EXCEPTION(
                                   "Future has been cancelled");
                           }

                           try {
                               if (sharedFuture->valid()) {
                                   return sharedFuture->get();
                               }
                               THROW_INVALID_FUTURE_EXCEPTION(
                                   "Future is invalid");
                           } catch (...) {
                               return func(std::current_exception());
                           }
                       })
                .share());
    }

    /**
     * @brief Cancels the future.
     */
    void cancel() noexcept { *cancelled_ = true; }

    /**
     * @brief Checks if the future has been cancelled.
     * @return True if the future has been cancelled, false otherwise.
     */
    [[nodiscard]] auto isCancelled() const noexcept -> bool {
        return *cancelled_;
    }

    /**
     * @brief Gets the exception associated with the future, if any.
     * @return A pointer to the exception, or nullptr if no exception.
     */
    auto getException() noexcept -> std::exception_ptr {
        try {
            future_.get();
        } catch (...) {
            return std::current_exception();
        }
        return nullptr;
    }

    /**
     * @brief Retries the operation associated with the future.
     * @tparam F The type of the function to call.
     * @param func The function to call when retrying.
     * @param max_retries The maximum number of retries.
     * @param backoff_ms Optional backoff time between retries (in milliseconds)
     * @return An EnhancedFuture for the result of the function.
     */
    template <ValidCallable<T> F>
    auto retry(F&& func, int max_retries,
               std::optional<int> backoff_ms = std::nullopt) {
        if (max_retries < 0) {
            THROW_INVALID_ARGUMENT("max_retries must be non-negative");
        }

        using ResultType = std::invoke_result_t<F, T>;
        auto sharedFuture = std::make_shared<std::shared_future<T>>(future_);
        auto sharedCancelled = cancelled_;

        return EnhancedFuture<ResultType>(
            std::async(
                std::launch::async,
                [sharedFuture, sharedCancelled, func = std::forward<F>(func),
                 max_retries, backoff_ms]() -> ResultType {
                    if (*sharedCancelled) {
                        THROW_INVALID_FUTURE_EXCEPTION(
                            "Future has been cancelled");
                    }

                    for (int attempt = 0; attempt < max_retries; ++attempt) {
                        if (!sharedFuture->valid()) {
                            THROW_INVALID_FUTURE_EXCEPTION("Future is invalid");
                        }

                        try {
                            return func(sharedFuture->get());
                        } catch (const std::exception& e) {
                            if (attempt == max_retries - 1) {
                                throw;  // Rethrow on last attempt
                            }

                            // Apply backoff if specified
                            if (backoff_ms.has_value()) {
                                std::this_thread::sleep_for(
                                    std::chrono::milliseconds(
                                        backoff_ms.value() * (attempt + 1)));
                            }
                        }
                    }

                    // Should never reach here if max_retries > 0
                    THROW_INVALID_FUTURE_EXCEPTION(
                        "Retry failed after maximum attempts");
                })
                .share());
    }

    auto isReady() const noexcept -> bool {
        return future_.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    }

    auto get() -> T {
        if (*cancelled_) {
            THROW_INVALID_FUTURE_EXCEPTION("Future has been cancelled");
        }
        return future_.get();
    }

    // C++20 coroutine support
    struct promise_type {
        std::promise<T> promise;

        auto get_return_object() noexcept -> EnhancedFuture<T> {
            return EnhancedFuture<T>(promise.get_future().share());
        }

        auto initial_suspend() noexcept -> std::suspend_never { return {}; }

        auto final_suspend() noexcept -> std::suspend_never { return {}; }

        template <typename U>
            requires std::convertible_to<U, T>
        void return_value(U&& value) {
            promise.set_value(std::forward<U>(value));
        }

        void unhandled_exception() {
            promise.set_exception(std::current_exception());
        }
    };

    /**
     * @brief Creates a coroutine awaiter for this future.
     * @return A coroutine awaiter object.
     */
    [[nodiscard]] auto operator co_await() const noexcept {
        return AwaitableEnhancedFuture<T>(future_);
    }

protected:
    std::shared_future<T> future_;  ///< The underlying shared future.
    std::shared_ptr<std::atomic<bool>>
        cancelled_;  ///< Flag indicating if the future has been cancelled.
#ifdef ATOM_USE_BOOST_LOCKFREE
    std::shared_ptr<LockfreeCallbackContainer>
        callbacks_;  ///< Lockfree container for callbacks.
#else
    std::shared_ptr<std::vector<std::function<void(T)>>>
        callbacks_;  ///< List of callbacks to be called on completion.
#endif
};

/**
 * @class EnhancedFuture<void>
 * @brief Specialization of the EnhancedFuture class for void type.
 */
template <>
class EnhancedFuture<void> {
public:
    // Enable coroutine support
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

#ifdef ATOM_USE_BOOST_LOCKFREE
    /**
     * @brief Callback wrapper for lockfree queue
     */
    struct CallbackWrapper {
        std::function<void()> callback;

        CallbackWrapper() = default;
        explicit CallbackWrapper(std::function<void()> cb)
            : callback(std::move(cb)) {}
    };

    /**
     * @brief Lockfree callback container for void return type
     */
    class LockfreeCallbackContainer {
    public:
        LockfreeCallbackContainer() : queue_(128) {}  // Default capacity

        void add(const std::function<void()>& callback) {
            auto* wrapper = new CallbackWrapper(callback);
            // Try pushing until successful
            while (!queue_.push(wrapper)) {
                std::this_thread::yield();
            }
        }

        void executeAll() {
            CallbackWrapper* wrapper = nullptr;
            while (queue_.pop(wrapper)) {
                if (wrapper && wrapper->callback) {
                    try {
                        wrapper->callback();
                    } catch (...) {
                        // Log error but continue with other callbacks
                    }
                    delete wrapper;
                }
            }
        }

        bool empty() const { return queue_.empty(); }

        ~LockfreeCallbackContainer() {
            CallbackWrapper* wrapper = nullptr;
            while (queue_.pop(wrapper)) {
                delete wrapper;
            }
        }

    private:
        boost::lockfree::queue<CallbackWrapper*> queue_;
    };
#endif

    /**
     * @brief Constructs an EnhancedFuture from a shared future.
     * @param fut The shared future to wrap.
     */
    explicit EnhancedFuture(std::shared_future<void>&& fut) noexcept
        : future_(std::move(fut)),
          cancelled_(std::make_shared<std::atomic<bool>>(false))
#ifdef ATOM_USE_BOOST_LOCKFREE
          ,
          callbacks_(std::make_shared<LockfreeCallbackContainer>())
#else
          ,
          callbacks_(std::make_shared<std::vector<std::function<void()>>>())
#endif
    {
    }

    explicit EnhancedFuture(const std::shared_future<void>& fut) noexcept
        : future_(fut),
          cancelled_(std::make_shared<std::atomic<bool>>(false))
#ifdef ATOM_USE_BOOST_LOCKFREE
          ,
          callbacks_(std::make_shared<LockfreeCallbackContainer>())
#else
          ,
          callbacks_(std::make_shared<std::vector<std::function<void()>>>())
#endif
    {
    }

    // Move constructor and assignment
    EnhancedFuture(EnhancedFuture&& other) noexcept = default;
    EnhancedFuture& operator=(EnhancedFuture&& other) noexcept = default;

    // Copy constructor and assignment
    EnhancedFuture(const EnhancedFuture&) = default;
    EnhancedFuture& operator=(const EnhancedFuture&) = default;

    /**
     * @brief Chains another operation to be called after the future is done.
     * @tparam F The type of the function to call.
     * @param func The function to call when the future is done.
     * @return An EnhancedFuture for the result of the function.
     */
    template <ValidCallable F>
    auto then(F&& func) {
        using ResultType = std::invoke_result_t<F>;
        auto sharedFuture = std::make_shared<std::shared_future<void>>(future_);
        auto sharedCancelled = cancelled_;

        return EnhancedFuture<ResultType>(
            std::async(std::launch::async,
                       [sharedFuture, sharedCancelled,
                        func = std::forward<F>(func)]() -> ResultType {
                           if (*sharedCancelled) {
                               THROW_INVALID_FUTURE_EXCEPTION(
                                   "Future has been cancelled");
                           }

                           if (sharedFuture->valid()) {
                               try {
                                   sharedFuture->get();
                                   return func();
                               } catch (...) {
                                   THROW_NESTED_INVALID_FUTURE_EXCEPTION(
                                       "Exception in then callback");
                               }
                           }
                           THROW_INVALID_FUTURE_EXCEPTION("Future is invalid");
                       })
                .share());
    }

    /**
     * @brief Waits for the future with a timeout and auto-cancels if not ready.
     * @param timeout The timeout duration.
     * @return True if the future is ready, false otherwise.
     */
    auto waitFor(std::chrono::milliseconds timeout) noexcept -> bool {
        if (future_.wait_for(timeout) == std::future_status::ready &&
            !*cancelled_) {
            try {
                future_.get();
                return true;
            } catch (...) {
                return false;
            }
        }
        cancel();
        return false;
    }

    /**
     * @brief Checks if the future is done.
     * @return True if the future is done, false otherwise.
     */
    [[nodiscard]] auto isDone() const noexcept -> bool {
        return future_.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    }

    /**
     * @brief Sets a completion callback to be called when the future is done.
     * @tparam F The type of the callback function.
     * @param func The callback function to add.
     */
    template <ValidCallable F>
    void onComplete(F&& func) {
        if (*cancelled_) {
            return;
        }

#ifdef ATOM_USE_BOOST_LOCKFREE
        callbacks_->add(std::forward<F>(func));

        std::thread([future = future_, callbacks = callbacks_,
                     cancelled = cancelled_]() mutable {
            try {
                if (!*cancelled && future.valid()) {
                    future.get();
                    if (!*cancelled) {
                        callbacks->executeAll();
                    }
                }
            } catch (...) {
                // Future completed with exception
            }
        }).detach();
#else
        callbacks_->emplace_back(std::forward<F>(func));

        std::thread([future = future_, callbacks = callbacks_,
                     cancelled = cancelled_]() mutable {
            try {
                if (!*cancelled && future.valid()) {
                    future.get();
                    for (auto& callback : *callbacks) {
                        try {
                            callback();
                        } catch (...) {
                            // Log error but continue with other callbacks
                        }
                    }
                }
            } catch (...) {
                // Future completed with exception
            }
        }).detach();
#endif
    }

    /**
     * @brief Waits synchronously for the future to complete.
     * @throws InvalidFutureException if the future is cancelled.
     */
    void wait() {
        if (*cancelled_) {
            THROW_INVALID_FUTURE_EXCEPTION("Future has been cancelled");
        }

        try {
            future_.get();
        } catch (const std::exception& e) {
            THROW_NESTED_INVALID_FUTURE_EXCEPTION(
                "Exception while waiting for future: ", e.what());
        } catch (...) {
            THROW_NESTED_INVALID_FUTURE_EXCEPTION(
                "Unknown exception while waiting for future");
        }
    }

    /**
     * @brief Cancels the future.
     */
    void cancel() noexcept { *cancelled_ = true; }

    /**
     * @brief Checks if the future has been cancelled.
     * @return True if the future has been cancelled, false otherwise.
     */
    [[nodiscard]] auto isCancelled() const noexcept -> bool {
        return *cancelled_;
    }

    /**
     * @brief Gets the exception associated with the future, if any.
     * @return A pointer to the exception, or nullptr if no exception.
     */
    auto getException() noexcept -> std::exception_ptr {
        try {
            future_.get();
        } catch (...) {
            return std::current_exception();
        }
        return nullptr;
    }

    auto isReady() const noexcept -> bool {
        return future_.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    }

    auto get() -> void {
        if (*cancelled_) {
            THROW_INVALID_FUTURE_EXCEPTION("Future has been cancelled");
        }
        future_.get();
    }

    // C++20 coroutine support
    struct promise_type {
        std::promise<void> promise;

        auto get_return_object() noexcept -> EnhancedFuture<void> {
            return EnhancedFuture<void>(promise.get_future().share());
        }

        auto initial_suspend() noexcept -> std::suspend_never { return {}; }

        auto final_suspend() noexcept -> std::suspend_never { return {}; }

        void return_void() noexcept { promise.set_value(); }

        void unhandled_exception() {
            promise.set_exception(std::current_exception());
        }
    };

    /**
     * @brief Creates a coroutine awaiter for this future.
     * @return A coroutine awaiter object.
     */
    [[nodiscard]] auto operator co_await() const noexcept {
        return AwaitableEnhancedFuture<void>(future_);
    }

protected:
    std::shared_future<void> future_;  ///< The underlying shared future.
    std::shared_ptr<std::atomic<bool>>
        cancelled_;  ///< Flag indicating if the future has been cancelled.
#ifdef ATOM_USE_BOOST_LOCKFREE
    std::shared_ptr<LockfreeCallbackContainer>
        callbacks_;  ///< Lockfree container for callbacks.
#else
    std::shared_ptr<std::vector<std::function<void()>>>
        callbacks_;  ///< List of callbacks to be called on completion.
#endif
};

/**
 * @brief Helper function to create an EnhancedFuture.
 * @tparam F The type of the function to call.
 * @tparam Args The types of the arguments to pass to the function.
 * @param f The function to call.
 * @param args The arguments to pass to the function.
 * @return An EnhancedFuture for the result of the function.
 */
template <typename F, typename... Args>
    requires ValidCallable<F, Args...>
auto makeEnhancedFuture(F&& f, Args&&... args) {
    using result_type = std::invoke_result_t<F, Args...>;
    return EnhancedFuture<result_type>(std::async(std::launch::async,
                                                  std::forward<F>(f),
                                                  std::forward<Args>(args)...)
                                           .share());
}

/**
 * @brief Helper function to get a future for a range of futures.
 * @tparam InputIt The type of the input iterator.
 * @param first The beginning of the range.
 * @param last The end of the range.
 * @param timeout An optional timeout duration.
 * @return A future containing a vector of the results of the input futures.
 */
template <std::input_iterator InputIt>
auto whenAll(InputIt first, InputIt last,
             std::optional<std::chrono::milliseconds> timeout = std::nullopt)
    -> std::future<
        std::vector<typename std::iterator_traits<InputIt>::value_type>> {
    using FutureType = typename std::iterator_traits<InputIt>::value_type;
    using ResultType = std::vector<FutureType>;

    // Validate input range
    if (std::distance(first, last) < 0) {
        THROW_INVALID_ARGUMENT("Invalid iterator range");
    }

    auto promise = std::make_shared<std::promise<ResultType>>();
    std::future<ResultType> resultFuture = promise->get_future();

    // Create an intermediate shared vector to store results
    auto results = std::make_shared<ResultType>();
    results->reserve(static_cast<size_t>(
        std::distance(first, last)));  // Pre-allocate memory

    // Use a thread to avoid blocking
    std::thread([promise, results, first, last, timeout]() mutable {
        try {
            for (auto it = first; it != last; ++it) {
                if (timeout) {
                    // Check each future with timeout (if specified)
                    if (it->wait_for(*timeout) == std::future_status::timeout) {
                        promise->set_exception(
                            std::make_exception_ptr(InvalidFutureException(
                                ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,
                                "Timeout while waiting for a future.")));
                        return;
                    }
                }
                results->push_back(std::move(*it));
            }
            promise->set_value(std::move(*results));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).detach();

    return resultFuture;
}

/**
 * @brief Helper to get the return type of a future.
 * @tparam T The type of the future.
 */
template <typename T>
using future_value_t = decltype(std::declval<T>().get());

/**
 * @brief Helper function for a variadic template version (when_all for futures
 * as arguments).
 * @tparam Futures The types of the futures.
 * @param futures The futures to wait for.
 * @return A future containing a tuple of the results of the input futures.
 * @throws InvalidFutureException if any future is invalid
 */
template <typename... Futures>
auto whenAll(Futures&&... futures)
    -> std::future<std::tuple<future_value_t<Futures>...>> {
    auto promise = std::make_shared<
        std::promise<std::tuple<future_value_t<Futures>...>>>();
    std::future<std::tuple<future_value_t<Futures>...>> resultFuture =
        promise->get_future();

    // Copy futures to a tuple to prevent dangling references
    auto futuresTuple = std::make_shared<std::tuple<std::decay_t<Futures>...>>(
        std::forward<Futures>(futures)...);

    // Use a thread to process futures asynchronously
    std::thread([promise, futuresTuple]() mutable {
        try {
            auto results = std::apply(
                [](auto&... fs) {
                    // Check if all futures are valid
                    if ((!fs.valid() || ...)) {
                        THROW_INVALID_FUTURE_EXCEPTION(
                            "One or more futures are invalid");
                    }
                    return std::make_tuple(fs.get()...);
                },
                *futuresTuple);
            promise->set_value(std::move(results));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).detach();

    return resultFuture;
}

// Helper function to create a coroutine-based EnhancedFuture
template <FutureCompatible T>
EnhancedFuture<T> co_makeEnhancedFuture(T value) {
    co_return value;
}

// Specialization for void
inline EnhancedFuture<void> co_makeEnhancedFuture() { co_return; }

// Utility to run parallel operations on a data collection using SIMD where
// applicable
template <std::ranges::input_range Range, typename Func>
    requires std::invocable<Func, std::ranges::range_value_t<Range>>
auto parallelProcess(Range&& range, Func&& func, size_t chunkSize = 0) {
    using ValueType = std::ranges::range_value_t<Range>;
    using ResultType = std::invoke_result_t<Func, ValueType>;

    // 根据平台自动确定最佳线程数和任务调度
#if defined(ATOM_PLATFORM_WINDOWS)
    if (chunkSize == 0) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        chunkSize = sysInfo.dwNumberOfProcessors;
    }
#elif defined(ATOM_PLATFORM_LINUX)
    if (chunkSize == 0) {
        chunkSize = get_nprocs();
    }
#else
    // 默认方法
    if (chunkSize == 0) {
        chunkSize =
            std::max(size_t(1),
                     static_cast<size_t>(std::thread::hardware_concurrency()));
    }
#endif

    std::vector<EnhancedFuture<ResultType>> futures;
    auto begin = std::ranges::begin(range);
    auto end = std::ranges::end(range);

    while (begin != end) {
        auto chunkEnd = begin;
        size_t distance = 0;

        while (chunkEnd != end && distance < chunkSize) {
            ++chunkEnd;
            ++distance;
        }

        futures.push_back(EnhancedFuture<ResultType>(
            std::async(std::launch::async,
                       [func = func, chunk = std::vector<ValueType>(
                                         begin, chunkEnd)]() -> ResultType {
                           // 处理分块
                           if (chunk.size() == 1) {
                               return func(chunk[0]);
                           } else {
                               ResultType result{};
                               for (const auto& item : chunk) {
                                   result = func(item);
                               }
                               return result;
                           }
                       })
                .share()));

        begin = chunkEnd;
    }

    return futures;
}

/**
 * @brief Create a thread pool optimized EnhancedFuture
 * @tparam F Function type
 * @tparam Args Parameter types
 * @param f Function to be called
 * @param args Parameters to pass to the function
 * @return EnhancedFuture of the function result
 */
template <typename F, typename... Args>
    requires ValidCallable<F, Args...>
auto makeOptimizedFuture(F&& f, Args&&... args) {
    using result_type = std::invoke_result_t<F, Args...>;

    // Use platform-specific thread pool optimization
#if defined(ATOM_PLATFORM_WINDOWS)
    // Windows thread pool implementation
    // This is just an example, actual projects may need a more complete
    // implementation
    return EnhancedFuture<result_type>(std::async(std::launch::async,
                                                  std::forward<F>(f),
                                                  std::forward<Args>(args)...)
                                           .share());
#elif defined(ATOM_PLATFORM_MACOS)
    // macOS uses GCD
    std::promise<result_type> promise;
    auto future = promise.get_future();

    // Create data structure to store function and parameters
    struct CallData {
        std::promise<result_type> promise;
        std::tuple<std::decay_t<F>, std::decay_t<Args>...> func_and_args;

        CallData(std::promise<result_type>&& p, F&& f, Args&&... args)
            : promise(std::move(p)),
              func_and_args(std::forward<F>(f), std::forward<Args>(args)...) {}

        static void execute(void* context) {
            auto* data = static_cast<CallData*>(context);
            try {
                if constexpr (std::is_void_v<result_type>) {
                    std::apply(
                        [](auto&&... args) {
                            std::invoke(std::forward<decltype(args)>(args)...);
                        },
                        data->func_and_args);
                    data->promise.set_value();
                } else {
                    data->promise.set_value(std::apply(
                        [](auto&&... args) {
                            return std::invoke(
                                std::forward<decltype(args)>(args)...);
                        },
                        data->func_and_args));
                }
            } catch (...) {
                data->promise.set_exception(std::current_exception());
            }
            delete data;
        }
    };

    auto* callData = new CallData(std::move(promise), std::forward<F>(f),
                                  std::forward<Args>(args)...);

    dispatch_async_f(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), callData,
        &CallData::execute);

    return EnhancedFuture<result_type>(future.share());
#else
    // Default standard implementation
    return EnhancedFuture<result_type>(std::async(std::launch::async,
                                                  std::forward<F>(f),
                                                  std::forward<Args>(args)...)
                                           .share());
#endif
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_FUTURE_HPP

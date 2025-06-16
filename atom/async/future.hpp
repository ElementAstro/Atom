#ifndef ATOM_ASYNC_FUTURE_HPP
#define ATOM_ASYNC_FUTURE_HPP

#include <algorithm>  // For std::max
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
#include <sys/sysinfo.h>  // For get_nprocs
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#endif

#ifdef ATOM_USE_ASIO
#include <asio/post.hpp>
#include <asio/thread_pool.hpp>
#include <mutex>  // For std::once_flag for thread_pool initialization
#endif

#include "atom/error/exception.hpp"

namespace atom::async {

/**
 * @brief Helper to get the return type of a future.
 * @tparam T The type of the future.
 */
template <typename T>
using future_value_t = decltype(std::declval<T>().get());

#ifdef ATOM_USE_ASIO
namespace internal {
inline asio::thread_pool& get_asio_thread_pool() {
    // Ensure thread pool is initialized safely and runs with a reasonable
    // number of threads
    static asio::thread_pool pool(
        std::max(1u, std::thread::hardware_concurrency() > 0
                         ? std::thread::hardware_concurrency()
                         : 2));
    return pool;
}
}  // namespace internal
#endif

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

// Concept to ensure a type can be used in a future
template <typename T>
concept FutureCompatible = std::is_object_v<T> || std::is_void_v<T>;

// Concept to ensure a callable can be used with specific arguments
template <typename F, typename... Args>
concept ValidCallable = requires(F&& f, Args&&... args) {
    { std::invoke(std::forward<F>(f), std::forward<Args>(args)...) };
};

// New: Coroutine awaitable helper class
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
#ifdef ATOM_USE_ASIO
        asio::post(atom::async::internal::get_asio_thread_pool(),
                   [future = future_, h = handle]() mutable {
                       future.wait();  // Wait in an Asio thread pool thread
                       h.resume();
                   });
#elif defined(ATOM_PLATFORM_WINDOWS)
        // Windows thread pool optimization (original comment)
        auto thread_proc = [](void* data) -> unsigned long {
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
            CreateThread(nullptr, 0, thread_proc, params, 0, nullptr);
        if (threadHandle) {
            CloseHandle(threadHandle);
        } else {
            // Handle thread creation failure, e.g., resume immediately or throw
            delete params;
            if (handle)
                handle.resume();  // Or signal error
        }
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
#ifdef ATOM_USE_ASIO
        asio::post(atom::async::internal::get_asio_thread_pool(),
                   [future = future_, h = handle]() mutable {
                       future.wait();  // Wait in an Asio thread pool thread
                       h.resume();
                   });
#elif defined(ATOM_PLATFORM_WINDOWS)
        auto thread_proc = [](void* data) -> unsigned long {
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
            CreateThread(nullptr, 0, thread_proc, params, 0, nullptr);
        if (threadHandle) {
            CloseHandle(threadHandle);
        } else {
            delete params;
            if (handle)
                handle.resume();
        }
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
                        // Consider adding spdlog here if available globally
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
#else
    // Mutex for std::vector based callbacks if ATOM_USE_BOOST_LOCKFREE is not
    // defined and onComplete can be called concurrently. For simplicity, this
    // example assumes external synchronization or non-concurrent calls to
    // onComplete for the std::vector case if not using Boost.Lockfree. If
    // concurrent calls to onComplete are expected for the std::vector path,
    // callbacks_ (the vector itself) would need a mutex for add and iteration.
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
            std::async(std::launch::async,  // This itself could use
                                            // makeOptimizedFuture
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
                                   THROW_INVALID_FUTURE_EXCEPTION(
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
        // Check if cancelPolicy is not the default empty std::function
        if constexpr (!std::is_same_v<std::decay_t<CancelFunc>,
                                      std::function<void()>> ||
                      (std::is_same_v<std::decay_t<CancelFunc>,
                                      std::function<void()>> &&
                       cancelPolicy)) {
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
        callbacks_->add(std::function<void(T)>(std::forward<F>(func)));
#else
        // For std::vector, ensure thread safety if onComplete is called
        // concurrently. This example assumes it's handled externally or not an
        // issue.
        callbacks_->emplace_back(std::forward<F>(func));
#endif

#ifdef ATOM_USE_ASIO
        asio::post(
            atom::async::internal::get_asio_thread_pool(),
            [future = future_, callbacks = callbacks_,
             cancelled = cancelled_]() mutable {
                try {
                    if (!*cancelled && future.valid()) {
                        T result =
                            future.get();  // Wait for the future in Asio thread
                        if (!*cancelled) {
#ifdef ATOM_USE_BOOST_LOCKFREE
                            callbacks->executeAll(result);
#else
                            // Iterate over the vector of callbacks.
                            // Assumes vector modifications are synchronized if
                            // they can occur.
                            for (auto& callback_fn : *callbacks) {
                                try {
                                    callback_fn(result);
                                } catch (...) {
                                    // Log error but continue
                                }
                            }
#endif
                        }
                    }
                } catch (...) {
                    // Future completed with exception
                }
            });
#else  // Original std::thread implementation
        std::thread([future = future_, callbacks = callbacks_,
                     cancelled = cancelled_]() mutable {
            try {
                if (!*cancelled && future.valid()) {
                    T result = future.get();
                    if (!*cancelled) {
#ifdef ATOM_USE_BOOST_LOCKFREE
                        callbacks->executeAll(result);
#else
                        for (auto& callback :
                             *callbacks) {  // Note: original captured callbacks
                                            // by value (shared_ptr copy)
                            try {
                                callback(result);
                            } catch (...) {
                                // Log error but continue with other callbacks
                            }
                        }
#endif
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
            THROW_INVALID_FUTURE_EXCEPTION(
                "Exception while waiting for future: ", e.what());
        } catch (...) {
            THROW_INVALID_FUTURE_EXCEPTION(
                "Unknown exception while waiting for future");
        }
    }

    template <ValidCallable<std::exception_ptr> F>
    auto catching(F&& func) {
        using ResultType = T;  // Assuming catching returns T or throws
        auto sharedFuture = std::make_shared<std::shared_future<T>>(future_);
        auto sharedCancelled = cancelled_;

        return EnhancedFuture<ResultType>(
            std::async(std::launch::async,  // This itself could use
                                            // makeOptimizedFuture
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
                               // If func rethrows or returns a different type,
                               // ResultType needs adjustment Assuming func
                               // returns T or throws, which is then caught by
                               // std::async's future
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
        if (isDone() && !*cancelled_) {  // Check if ready to avoid blocking
            try {
                future_.get();  // This re-throws if future stores an exception
            } catch (...) {
                return std::current_exception();
            }
        } else if (*cancelled_) {
            // Optionally return a specific exception for cancelled futures
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
            std::async(  // This itself could use makeOptimizedFuture
                std::launch::async,
                [sharedFuture, sharedCancelled, func = std::forward<F>(func),
                 max_retries, backoff_ms]() -> ResultType {
                    if (*sharedCancelled) {
                        THROW_INVALID_FUTURE_EXCEPTION(
                            "Future has been cancelled");
                    }

                    for (int attempt = 0; attempt <= max_retries;
                         ++attempt) {  // <= to allow max_retries attempts
                        if (!sharedFuture->valid()) {
                            // This check might be problematic if the original
                            // future is single-use and already .get() Assuming
                            // 'func' takes the result of the *original* future.
                            // If 'func' is the operation to retry, this
                            // structure is different. The current structure
                            // implies 'func' processes the result of
                            // 'sharedFuture'. A retry typically means
                            // re-executing the operation that *produced*
                            // sharedFuture. This 'retry' seems to retry
                            // processing its result. For clarity, let's assume
                            // 'func' is a processing step.
                            THROW_INVALID_FUTURE_EXCEPTION(
                                "Future is invalid for retry processing");
                        }

                        try {
                            // This implies the original future should be
                            // get-able multiple times, or func is retrying
                            // based on a single result. If sharedFuture.get()
                            // throws, the catch block is hit.
                            return func(sharedFuture->get());
                        } catch (const std::exception& e) {
                            if (attempt == max_retries) {
                                throw;  // Rethrow on last attempt
                            }
                            // Log attempt failure: spdlog::warn("Retry attempt
                            // {} failed: {}", attempt, e.what());
                            if (backoff_ms.has_value()) {
                                std::this_thread::sleep_for(
                                    std::chrono::milliseconds(
                                        backoff_ms.value() *
                                        (attempt +
                                         1)));  // Consider exponential backoff
                            }
                        }
                        if (*sharedCancelled) {  // Check cancellation between
                                                 // retries
                            THROW_INVALID_FUTURE_EXCEPTION(
                                "Future cancelled during retry");
                        }
                    }
                    // Should not be reached if max_retries >= 0
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
                        // Log error
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

    EnhancedFuture(EnhancedFuture&& other) noexcept = default;
    EnhancedFuture& operator=(EnhancedFuture&& other) noexcept = default;
    EnhancedFuture(const EnhancedFuture&) = default;
    EnhancedFuture& operator=(const EnhancedFuture&) = default;

    template <ValidCallable F>
    auto then(F&& func) {
        using ResultType = std::invoke_result_t<F>;
        auto sharedFuture = std::make_shared<std::shared_future<void>>(future_);
        auto sharedCancelled = cancelled_;

        return EnhancedFuture<ResultType>(
            std::async(std::launch::async,  // This itself could use
                                            // makeOptimizedFuture
                       [sharedFuture, sharedCancelled,
                        func = std::forward<F>(func)]() -> ResultType {
                           if (*sharedCancelled) {
                               THROW_INVALID_FUTURE_EXCEPTION(
                                   "Future has been cancelled");
                           }
                           if (sharedFuture->valid()) {
                               try {
                                   sharedFuture->get();  // Wait for void future
                                   return func();
                               } catch (...) {
                                   THROW_INVALID_FUTURE_EXCEPTION(
                                       "Exception in then callback");
                               }
                           }
                           THROW_INVALID_FUTURE_EXCEPTION("Future is invalid");
                       })
                .share());
    }

    auto waitFor(std::chrono::milliseconds timeout) noexcept -> bool {
        if (future_.wait_for(timeout) == std::future_status::ready &&
            !*cancelled_) {
            try {
                future_.get();
                return true;
            } catch (...) {
                return false;  // Exception during get
            }
        }
        cancel();
        return false;
    }

    [[nodiscard]] auto isDone() const noexcept -> bool {
        return future_.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    }

    template <ValidCallable F>
    void onComplete(F&& func) {
        if (*cancelled_) {
            return;
        }

#ifdef ATOM_USE_BOOST_LOCKFREE
        callbacks_->add(std::function<void()>(std::forward<F>(func)));
#else
        callbacks_->emplace_back(std::forward<F>(func));
#endif

#ifdef ATOM_USE_ASIO
        asio::post(atom::async::internal::get_asio_thread_pool(),
                   [future = future_, callbacks = callbacks_,
                    cancelled = cancelled_]() mutable {
                       try {
                           if (!*cancelled && future.valid()) {
                               future.get();  // Wait for void future
                               if (!*cancelled) {
#ifdef ATOM_USE_BOOST_LOCKFREE
                                   callbacks->executeAll();
#else
                        for (auto& callback_fn : *callbacks) {
                            try {
                                callback_fn();
                            } catch (...) {
                                // Log error
                            }
                        }
#endif
                               }
                           }
                       } catch (...) {
                           // Future completed with exception
                       }
                   });
#else  // Original std::thread implementation
        std::thread([future = future_, callbacks = callbacks_,
                     cancelled = cancelled_]() mutable {
            try {
                if (!*cancelled && future.valid()) {
                    future.get();
                    if (!*cancelled) {
#ifdef ATOM_USE_BOOST_LOCKFREE
                        callbacks->executeAll();
#else
                        for (auto& callback : *callbacks) {
                            try {
                                callback();
                            } catch (...) {
                                // Log error
                            }
                        }
#endif
                    }
                }
            } catch (...) {
                // Future completed with exception
            }
        }).detach();
#endif
    }

    void wait() {
        if (*cancelled_) {
            THROW_INVALID_FUTURE_EXCEPTION("Future has been cancelled");
        }
        try {
            future_.get();
        } catch (const std::exception& e) {
            THROW_INVALID_FUTURE_EXCEPTION(  // Corrected macro
                "Exception while waiting for future: ", e.what());
        } catch (...) {
            THROW_INVALID_FUTURE_EXCEPTION(  // Corrected macro
                "Unknown exception while waiting for future");
        }
    }

    void cancel() noexcept { *cancelled_ = true; }
    [[nodiscard]] auto isCancelled() const noexcept -> bool {
        return *cancelled_;
    }

    auto getException() noexcept -> std::exception_ptr {
        if (isDone() && !*cancelled_) {
            try {
                future_.get();
            } catch (...) {
                return std::current_exception();
            }
        }
        return nullptr;
    }

    auto isReady() const noexcept -> bool {
        return future_.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    }

    void get() {  // Renamed from wait to get for void, or keep wait? 'get' is
                  // more std::future like.
        if (*cancelled_) {
            THROW_INVALID_FUTURE_EXCEPTION("Future has been cancelled");
        }
        future_.get();
    }

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
    std::shared_future<void> future_;
    std::shared_ptr<std::atomic<bool>> cancelled_;
#ifdef ATOM_USE_BOOST_LOCKFREE
    std::shared_ptr<LockfreeCallbackContainer> callbacks_;
#else
    std::shared_ptr<std::vector<std::function<void()>>> callbacks_;
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
    // Forward to makeOptimizedFuture to use potential Asio or platform
    // optimizations
    return makeOptimizedFuture(std::forward<F>(f), std::forward<Args>(args)...);
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
    -> std::future<std::vector<
        typename std::iterator_traits<InputIt>::value_type::value_type>> {
    using EnhancedFutureType =
        typename std::iterator_traits<InputIt>::value_type;
    using ValueType = decltype(std::declval<EnhancedFutureType>().get());
    using ResultType = std::vector<ValueType>;

    if (std::distance(first, last) < 0) {
        THROW_INVALID_ARGUMENT("Invalid iterator range");
    }
    if (first == last) {
        std::promise<ResultType> promise;
        promise.set_value({});
        return promise.get_future();
    }

    auto promise_ptr = std::make_shared<std::promise<ResultType>>();
    std::future<ResultType> resultFuture = promise_ptr->get_future();

    auto results_ptr = std::make_shared<ResultType>();
    size_t total_count = static_cast<size_t>(std::distance(first, last));
    results_ptr->reserve(total_count);

    auto futures_vec =
        std::make_shared<std::vector<EnhancedFutureType>>(first, last);

    auto temp_results =
        std::make_shared<std::vector<std::optional<ValueType>>>(total_count);
    auto promise_fulfilled = std::make_shared<std::atomic<bool>>(false);

    std::thread([promise_ptr, results_ptr, futures_vec, timeout, total_count,
                 temp_results, promise_fulfilled]() mutable {
        try {
            for (size_t i = 0; i < total_count; ++i) {
                auto& fut = (*futures_vec)[i];
                if (timeout.has_value()) {
                    if (fut.isReady()) {
                        // already ready
                    } else {
                        // EnhancedFuture::waitFor returns std::optional<T>
                        // If it returns nullopt, it means timeout or error
                        // during its own get().
                        auto opt_val = fut.waitFor(timeout.value());
                        if (!opt_val.has_value() && !fut.isReady()) {
                            if (!promise_fulfilled->exchange(true)) {
                                promise_ptr->set_exception(
                                    std::make_exception_ptr(
                                        InvalidFutureException(
                                            ATOM_FILE_NAME, ATOM_FILE_LINE,
                                            ATOM_FUNC_NAME,
                                            "Timeout while waiting for a "
                                            "future in whenAll.")));
                            }
                            return;
                        }
                        // If fut.isReady() is true here, it means it completed.
                        // The value from opt_val is not directly used here,
                        // fut.get() below will retrieve it or rethrow.
                    }
                }

                if constexpr (std::is_void_v<ValueType>) {
                    fut.get();
                    (*temp_results)[i].emplace();
                } else {
                    (*temp_results)[i] = fut.get();
                }
            }

            if (!promise_fulfilled->exchange(true)) {
                if constexpr (std::is_void_v<ValueType>) {
                    results_ptr->resize(total_count);
                } else {
                    results_ptr->clear();
                    for (size_t i = 0; i < total_count; ++i) {
                        if ((*temp_results)[i].has_value()) {
                            results_ptr->push_back(*(*temp_results)[i]);
                        }
                        // If a non-void future's result was not set in
                        // temp_results, it implies an issue, as fut.get()
                        // should have thrown if it failed. For correctly
                        // completed non-void futures, has_value() should be
                        // true.
                    }
                }
                promise_ptr->set_value(std::move(*results_ptr));
            }
        } catch (...) {
            if (!promise_fulfilled->exchange(true)) {
                promise_ptr->set_exception(std::current_exception());
            }
        }
    }).detach();

    return resultFuture;
}

/**
 * @brief Helper function for a variadic template version (when_all for futures
 * as arguments).
 * @tparam Futures The types of the futures.
 * @param futures The futures to wait for.
 * @return A future containing a tuple of the results of the input futures.
 * @throws InvalidFutureException if any future is invalid
 */
template <typename... Futures>
    requires(FutureCompatible<future_value_t<std::decay_t<Futures>>> &&
             ...)  // Ensure results are FutureCompatible
auto whenAll(Futures&&... futures) -> std::future<
    std::tuple<future_value_t<std::decay_t<Futures>>...>> {  // Ensure decay for
                                                             // future_value_t

    auto promise = std::make_shared<
        std::promise<std::tuple<future_value_t<std::decay_t<Futures>>...>>>();
    std::future<std::tuple<future_value_t<std::decay_t<Futures>>...>>
        resultFuture = promise->get_future();

    auto futuresTuple = std::make_shared<std::tuple<std::decay_t<Futures>...>>(
        std::forward<Futures>(futures)...);

    std::thread([promise,
                 futuresTuple]() mutable {  // Could use makeOptimizedFuture for
                                            // this thread
        try {
            // Check validity before calling get()
            std::apply(
                [](auto&... fs) {
                    if (((!fs.isReady() && !fs.isCancelled() && !fs.valid()) ||
                         ...)) {
                        // For EnhancedFuture, check isReady() or isCancelled()
                        // A more generic check: if it's not done and not going
                        // to be done. This check needs to be adapted for
                        // EnhancedFuture's interface. For now, assume .get()
                        // will throw if invalid.
                    }
                },
                *futuresTuple);

            auto results = std::apply(
                [](auto&... fs) {
                    // Original check: if ((!fs.valid() || ...))
                    // For EnhancedFuture, valid() is not the primary check.
                    // isCancelled() or get() throwing is. The .get() method in
                    // EnhancedFuture already checks for cancellation.
                    return std::make_tuple(fs.get()...);
                },
                *futuresTuple);
            promise->set_value(std::move(results));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    })
        .detach();

    return resultFuture;
}

// Helper function to create a coroutine-based EnhancedFuture
template <FutureCompatible T>
EnhancedFuture<T> co_makeEnhancedFuture(T value) {
    co_return value;
}

// Specialization for void
inline EnhancedFuture<void> co_makeEnhancedFuture() { co_return; }

// Utility to run parallel operations on a data collection
template <std::ranges::input_range Range, typename Func>
    requires std::invocable<Func, std::ranges::range_value_t<Range>>
auto parallelProcess(Range&& range, Func&& func, size_t numTasks = 0) {
    using ValueType = std::ranges::range_value_t<Range>;
    using SingleItemResultType = std::invoke_result_t<Func, ValueType>;
    using TaskChunkResultType =
        std::conditional_t<std::is_void_v<SingleItemResultType>, void,
                           std::vector<SingleItemResultType>>;

    if (numTasks == 0) {
#if defined(ATOM_PLATFORM_WINDOWS)
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numTasks = sysInfo.dwNumberOfProcessors;
#elif defined(ATOM_PLATFORM_LINUX)
        numTasks = get_nprocs();
#elif defined(__APPLE__)
        numTasks =
            std::max(size_t(1),
                     static_cast<size_t>(std::thread::hardware_concurrency()));
#else
        numTasks =
            std::max(size_t(1),
                     static_cast<size_t>(std::thread::hardware_concurrency()));
#endif
        if (numTasks == 0) {
            numTasks = 2;
        }
    }

    std::vector<EnhancedFuture<TaskChunkResultType>> futures;
    auto begin = std::ranges::begin(range);
    auto end = std::ranges::end(range);
    size_t totalSize = static_cast<size_t>(std::ranges::distance(range));

    if (totalSize == 0) {
        return futures;
    }

    size_t itemsPerTask = (totalSize + numTasks - 1) / numTasks;

    for (size_t i = 0; i < numTasks && begin != end; ++i) {
        auto task_begin = begin;
        auto task_end = std::ranges::next(
            task_begin,
            std::min(itemsPerTask, static_cast<size_t>(
                                       std::ranges::distance(task_begin, end))),
            end);

        std::vector<ValueType> local_chunk(task_begin, task_end);
        if (local_chunk.empty()) {
            continue;
        }

        futures.push_back(makeOptimizedFuture(
            [func = std::forward<Func>(func),
             local_chunk = std::move(local_chunk)]() -> TaskChunkResultType {
                if constexpr (std::is_void_v<SingleItemResultType>) {
                    for (const auto& item : local_chunk) {
                        func(item);
                    }
                    return;
                } else {
                    std::vector<SingleItemResultType> chunk_results;
                    chunk_results.reserve(local_chunk.size());
                    for (const auto& item : local_chunk) {
                        chunk_results.push_back(func(item));
                    }
                    return chunk_results;
                }
            }));
        begin = task_end;
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

#ifdef ATOM_USE_ASIO
    std::promise<result_type> promise;
    auto future = promise.get_future();

    asio::post(
        atom::async::internal::get_asio_thread_pool(),
        // Capture arguments carefully for the task
        [p = std::move(promise), func_capture = std::forward<F>(f),
         args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            try {
                if constexpr (std::is_void_v<result_type>) {
                    std::apply(func_capture, std::move(args_tuple));
                    p.set_value();
                } else {
                    p.set_value(
                        std::apply(func_capture, std::move(args_tuple)));
                }
            } catch (...) {
                p.set_exception(std::current_exception());
            }
        });
    return EnhancedFuture<result_type>(future.share());

#elif defined(ATOM_PLATFORM_MACOS) && \
    !defined(ATOM_USE_ASIO)  // Ensure ATOM_USE_ASIO takes precedence
    std::promise<result_type> promise;
    auto future = promise.get_future();

    struct CallData {
        std::promise<result_type> promise;
        // Use a std::function or store f and args separately if they are not
        // easily stored in a tuple or decay issues. For simplicity, assuming
        // they can be moved/copied into a lambda or struct.
        std::function<void()> work;  // Type erase the call

        template <typename F_inner, typename... Args_inner>
        CallData(std::promise<result_type>&& p, F_inner&& f_inner,
                 Args_inner&&... args_inner)
            : promise(std::move(p)) {
            work = [this, f_capture = std::forward<F_inner>(f_inner),
                    args_capture_tuple = std::make_tuple(
                        std::forward<Args_inner>(args_inner)...)]() mutable {
                try {
                    if constexpr (std::is_void_v<result_type>) {
                        std::apply(f_capture, std::move(args_capture_tuple));
                        this->promise.set_value();
                    } else {
                        this->promise.set_value(std::apply(
                            f_capture, std::move(args_capture_tuple)));
                    }
                } catch (...) {
                    this->promise.set_exception(std::current_exception());
                }
            };
        }
        static void execute(void* context) {
            auto* data = static_cast<CallData*>(context);
            data->work();
            delete data;
        }
    };
    auto* callData = new CallData(std::move(promise), std::forward<F>(f),
                                  std::forward<Args>(args)...);
    dispatch_async_f(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), callData,
        &CallData::execute);
    return EnhancedFuture<result_type>(future.share());

#else  // Default to std::async (covers Windows if not ATOM_USE_ASIO, and
       // generic Linux)
    return EnhancedFuture<result_type>(std::async(std::launch::async,
                                                  std::forward<F>(f),
                                                  std::forward<Args>(args)...)
                                           .share());
#endif
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_FUTURE_HPP

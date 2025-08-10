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
#include <tuple>  // For std::apply
#include <type_traits>
#include <vector>

#include <spdlog/spdlog.h>  // For logging

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
/**
 * @brief Returns a reference to the global Asio thread pool.
 * @return asio::thread_pool& The Asio thread pool.
 */
inline asio::thread_pool& get_asio_thread_pool() {
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

/**
 * @brief Awaitable helper class for EnhancedFuture to support C++20 coroutines.
 * @tparam T The type of the value the future holds.
 */
template <typename T>
class [[nodiscard]] AwaitableEnhancedFuture {
public:
    /**
     * @brief Constructs an AwaitableEnhancedFuture.
     * @param future The shared_future to await.
     */
    explicit AwaitableEnhancedFuture(std::shared_future<T> future)
        : future_(std::move(future)) {}

    /**
     * @brief Checks if the awaitable is ready without blocking.
     * @return true if the future is ready, false otherwise.
     */
    bool await_ready() const noexcept {
        return future_.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready;
    }

    /**
     * @brief Suspends the coroutine and schedules its resumption when the
     * future is ready.
     * @tparam Promise The promise type of the coroutine.
     * @param handle The coroutine handle to resume.
     */
    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) const {
#ifdef ATOM_USE_ASIO
        asio::post(atom::async::internal::get_asio_thread_pool(),
                   [future = future_, h = handle]() mutable {
                       future.wait();
                       h.resume();
                   });
#elif defined(ATOM_PLATFORM_WINDOWS)
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
            spdlog::error(
                "Failed to create thread for await_suspend on Windows.");
            delete params;
            if (handle)
                handle.resume();
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

    /**
     * @brief Retrieves the result of the awaited future.
     * @return The value of the future.
     */
    T await_resume() const { return future_.get(); }

private:
    std::shared_future<T> future_;
};

/**
 * @brief Specialization of AwaitableEnhancedFuture for void type.
 */
template <>
class [[nodiscard]] AwaitableEnhancedFuture<void> {
public:
    /**
     * @brief Constructs an AwaitableEnhancedFuture for void.
     * @param future The shared_future<void> to await.
     */
    explicit AwaitableEnhancedFuture(std::shared_future<void> future)
        : future_(std::move(future)) {}

    /**
     * @brief Checks if the awaitable is ready without blocking.
     * @return true if the future is ready, false otherwise.
     */
    bool await_ready() const noexcept {
        return future_.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready;
    }

    /**
     * @brief Suspends the coroutine and schedules its resumption when the
     * future is ready.
     * @tparam Promise The promise type of the coroutine.
     * @param handle The coroutine handle to resume.
     */
    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) const {
#ifdef ATOM_USE_ASIO
        asio::post(atom::async::internal::get_asio_thread_pool(),
                   [future = future_, h = handle]() mutable {
                       future.wait();
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
            spdlog::error(
                "Failed to create thread for await_suspend on Windows.");
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

    /**
     * @brief Resumes the coroutine after the future completes.
     */
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
    /**
     * @brief Promise type for coroutine support.
     */
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

#ifdef ATOM_USE_BOOST_LOCKFREE
    /**
     * @brief Callback wrapper for lockfree queue.
     */
    struct CallbackWrapper {
        std::function<void(T)> callback;

        CallbackWrapper() = default;
        explicit CallbackWrapper(std::function<void(T)> cb)
            : callback(std::move(cb)) {}
    };

    /**
     * @brief Lockfree callback container.
     */
    class LockfreeCallbackContainer {
    public:
        /**
         * @brief Constructs a LockfreeCallbackContainer.
         */
        LockfreeCallbackContainer() : queue_(128) {}  // Default capacity

        /**
         * @brief Adds a callback to the container.
         * @param callback The callback function.
         */
        void add(const std::function<void(T)>& callback) {
            auto* wrapper = new CallbackWrapper(callback);
            while (!queue_.push(wrapper)) {
                std::this_thread::yield();  // Yield to allow other threads to
                                            // progress
            }
        }

        /**
         * @brief Executes all stored callbacks with the given value.
         * @param value The value to pass to the callbacks.
         */
        void executeAll(const T& value) {
            CallbackWrapper* wrapper = nullptr;
            while (queue_.pop(wrapper)) {
                if (wrapper && wrapper->callback) {
                    try {
                        wrapper->callback(value);
                    } catch (const std::exception& e) {
                        spdlog::error("Exception in onComplete callback: {}",
                                      e.what());
                    } catch (...) {
                        spdlog::error(
                            "Unknown exception in onComplete callback.");
                    }
                    delete wrapper;
                }
            }
        }

        /**
         * @brief Checks if the container is empty.
         * @return true if empty, false otherwise.
         */
        bool empty() const { return queue_.empty(); }

        /**
         * @brief Destroys the LockfreeCallbackContainer and cleans up remaining
         * wrappers.
         */
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
    // For std::vector based callbacks, a mutex is required for thread-safety
    // if onComplete can be called concurrently.
    // This mutex should be part of the shared state, not the EnhancedFuture
    // object itself.
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
          ,  // Initialize callbacks_mutex_ptr_ here
          callbacks_mutex_ptr_(std::make_shared<std::mutex>()),
          callbacks_(std::make_shared<std::vector<std::function<void(T)>>>())
#endif
    {
    }

    /**
     * @brief Constructs an EnhancedFuture from a shared future.
     * @param fut The shared future to wrap.
     */
    explicit EnhancedFuture(const std::shared_future<T>& fut) noexcept
        : future_(fut),
          cancelled_(std::make_shared<std::atomic<bool>>(false))
#ifdef ATOM_USE_BOOST_LOCKFREE
          ,
          callbacks_(std::make_shared<LockfreeCallbackContainer>())
#else
          ,  // Initialize callbacks_mutex_ptr_ here
          callbacks_mutex_ptr_(std::make_shared<std::mutex>()),
          callbacks_(std::make_shared<std::vector<std::function<void(T)>>>())
#endif
    {
    }

    /**
     * @brief Move constructor.
     * @param other The other EnhancedFuture to move from.
     */
    EnhancedFuture(EnhancedFuture&& other) noexcept = default;
    /**
     * @brief Move assignment operator.
     * @param other The other EnhancedFuture to move from.
     * @return A reference to this EnhancedFuture.
     */
    EnhancedFuture& operator=(EnhancedFuture&& other) noexcept = default;

    /**
     * @brief Copy constructor.
     * @param other The other EnhancedFuture to copy from.
     */
    EnhancedFuture(const EnhancedFuture&) = default;
    /**
     * @brief Copy assignment operator.
     * @param other The other EnhancedFuture to copy from.
     * @return A reference to this EnhancedFuture.
     */
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
        auto sharedCancelled = cancelled_;

        return EnhancedFuture<ResultType>(
            std::async(
                std::launch::async,
                [sharedFuture, sharedCancelled,
                 func = std::forward<F>(func)]() -> ResultType {
                    if (*sharedCancelled) {
                        spdlog::warn(
                            "Then callback skipped: Future was cancelled.");
                        THROW_INVALID_FUTURE_EXCEPTION(
                            "Future has been cancelled");
                    }

                    if (sharedFuture->valid()) {
                        try {
                            return func(sharedFuture->get());
                        } catch (const std::exception& e) {
                            spdlog::error("Exception in then callback: {}",
                                          e.what());
                            THROW_INVALID_FUTURE_EXCEPTION(
                                "Exception in then callback");
                        } catch (...) {
                            spdlog::error(
                                "Unknown exception in then callback.");
                            THROW_INVALID_FUTURE_EXCEPTION(
                                "Unknown exception in then callback");
                        }
                    }
                    spdlog::error("Then callback failed: Future is invalid.");
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
            } catch (const std::exception& e) {
                spdlog::error("Exception during waitFor get: {}", e.what());
                return std::nullopt;
            } catch (...) {
                spdlog::error("Unknown exception during waitFor get.");
                return std::nullopt;
            }
        }
        cancel();  // Cancel if not ready within timeout
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
            } catch (const std::exception& e) {
                spdlog::error(
                    "Exception during waitFor get with custom policy: {}",
                    e.what());
                return std::nullopt;
            } catch (...) {
                spdlog::error(
                    "Unknown exception during waitFor get with custom policy.");
                return std::nullopt;
            }
        }

        cancel();
        if constexpr (!std::is_same_v<std::decay_t<CancelFunc>,
                                      std::function<void()>> ||
                      (std::is_same_v<std::decay_t<CancelFunc>,
                                      std::function<void()>> &&
                       cancelPolicy)) {
            try {
                std::invoke(std::forward<CancelFunc>(cancelPolicy));
            } catch (const std::exception& e) {
                spdlog::error("Exception in custom cancel policy: {}",
                              e.what());
            } catch (...) {
                spdlog::error("Unknown exception in custom cancel policy.");
            }
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
            spdlog::warn(
                "onComplete callback not added: Future already cancelled.");
            return;
        }

#ifdef ATOM_USE_BOOST_LOCKFREE
        callbacks_->add(std::function<void(T)>(std::forward<F>(func)));
#else
        {
            std::lock_guard<std::mutex> lock(*callbacks_mutex_ptr_);
            callbacks_->emplace_back(std::forward<F>(func));
        }
#endif

#ifdef ATOM_USE_ASIO
        asio::post(
            atom::async::internal::get_asio_thread_pool(),
            [future = future_, callbacks = callbacks_, cancelled = cancelled_
#ifndef ATOM_USE_BOOST_LOCKFREE
             ,
             callbacks_mutex_ptr =
                 callbacks_mutex_ptr_  // Capture the shared_ptr to mutex
#endif
        ]() mutable {
                try {
                    if (!*cancelled && future.valid()) {
                        T result =
                            future.get();  // Wait for the future in Asio thread
                        if (!*cancelled) {
#ifdef ATOM_USE_BOOST_LOCKFREE
                            callbacks->executeAll(result);
#else
                            std::lock_guard<std::mutex> lock(
                                *callbacks_mutex_ptr);  // Lock for iteration
                            for (auto& callback_fn : *callbacks) {
                                try {
                                    callback_fn(result);
                                } catch (const std::exception& e) {
                                    spdlog::error(
                                        "Exception in onComplete callback "
                                        "(vector): {}",
                                        e.what());
                                } catch (...) {
                                    spdlog::error(
                                        "Unknown exception in onComplete "
                                        "callback (vector).");
                                }
                            }
#endif
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::warn(
                        "Future completed with exception in onComplete "
                        "handler: {}",
                        e.what());
                } catch (...) {
                    spdlog::warn(
                        "Future completed with unknown exception in onComplete "
                        "handler.");
                }
            });
#else  // Original std::thread implementation
        std::thread([future = future_, callbacks = callbacks_,
                     cancelled = cancelled_
#ifndef ATOM_USE_BOOST_LOCKFREE
                     ,
                     callbacks_mutex_ptr =
                         callbacks_mutex_ptr_  // Capture shared_ptr to mutex
#endif
        ]() mutable {
            try {
                if (!*cancelled && future.valid()) {
                    T result = future.get();
                    if (!*cancelled) {
#ifdef ATOM_USE_BOOST_LOCKFREE
                        callbacks->executeAll(result);
#else
                        std::lock_guard<std::mutex> lock(
                            *callbacks_mutex_ptr);  // Lock for iteration
                        for (auto& callback : *callbacks) {
                            try {
                                callback(result);
                            } catch (const std::exception& e) {
                                spdlog::error(
                                    "Exception in onComplete callback "
                                    "(vector): {}",
                                    e.what());
                            } catch (...) {
                                spdlog::error(
                                    "Unknown exception in onComplete callback "
                                    "(vector).");
                            }
                        }
#endif
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn(
                    "Future completed with exception in onComplete handler: {}",
                    e.what());
            } catch (...) {
                spdlog::warn(
                    "Future completed with unknown exception in onComplete "
                    "handler.");
            }
        }).detach();
#endif
    }

    /**
     * @brief Waits synchronously for the future to complete.
     * @return The value of the future.
     * @throws InvalidFutureException if the future is cancelled or an exception
     * occurs.
     */
    auto wait() -> T {
        if (*cancelled_) {
            spdlog::error("Attempted to wait on a cancelled future.");
            THROW_INVALID_FUTURE_EXCEPTION("Future has been cancelled");
        }

        try {
            return future_.get();
        } catch (const std::exception& e) {
            spdlog::error("Exception while waiting for future: {}", e.what());
            THROW_INVALID_FUTURE_EXCEPTION(
                "Exception while waiting for future: ", e.what());
        } catch (...) {
            spdlog::error("Unknown exception while waiting for future.");
            THROW_INVALID_FUTURE_EXCEPTION(
                "Unknown exception while waiting for future");
        }
    }

    /**
     * @brief Handles exceptions from the future.
     * @tparam F The type of the exception handling function.
     * @param func The function to call with the exception_ptr.
     * @return An EnhancedFuture for the result.
     */
    template <ValidCallable<std::exception_ptr> F>
    auto catching(F&& func) {
        using ResultType = T;
        auto sharedFuture = std::make_shared<std::shared_future<T>>(future_);
        auto sharedCancelled = cancelled_;

        return EnhancedFuture<ResultType>(
            std::async(
                std::launch::async,
                [sharedFuture, sharedCancelled,
                 func = std::forward<F>(func)]() -> ResultType {
                    if (*sharedCancelled) {
                        spdlog::warn(
                            "Catching callback skipped: Future was cancelled.");
                        THROW_INVALID_FUTURE_EXCEPTION(
                            "Future has been cancelled");
                    }

                    try {
                        if (sharedFuture->valid()) {
                            return sharedFuture->get();
                        }
                        spdlog::error(
                            "Catching callback failed: Future is invalid.");
                        THROW_INVALID_FUTURE_EXCEPTION("Future is invalid");
                    } catch (...) {
                        return func(std::current_exception());
                    }
                })
                .share());
    }

    /**
     * @brief Cancels the future.
     */
    void cancel() noexcept {
        if (!*cancelled_) {
            *cancelled_ = true;
            spdlog::debug("Future cancelled.");
        }
    }

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
        if (isDone() && !*cancelled_) {
            try {
                future_.get();
            } catch (...) {
                return std::current_exception();
            }
        } else if (*cancelled_) {
            spdlog::debug(
                "Attempted to get exception from a cancelled future.");
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
            spdlog::error(
                "Invalid argument: max_retries must be non-negative.");
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
                        spdlog::warn(
                            "Retry operation skipped: Future was cancelled.");
                        THROW_INVALID_FUTURE_EXCEPTION(
                            "Future has been cancelled");
                    }

                    for (int attempt = 0; attempt <= max_retries; ++attempt) {
                        if (!sharedFuture->valid()) {
                            spdlog::error(
                                "Future invalid during retry processing.");
                            THROW_INVALID_FUTURE_EXCEPTION(
                                "Future is invalid for retry processing");
                        }

                        try {
                            return func(sharedFuture->get());
                        } catch (const std::exception& e) {
                            spdlog::warn("Retry attempt {} failed: {}",
                                         attempt + 1, e.what());
                            if (attempt == max_retries) {
                                throw;
                            }
                            if (backoff_ms.has_value()) {
                                std::this_thread::sleep_for(
                                    std::chrono::milliseconds(
                                        backoff_ms.value() * (attempt + 1)));
                            }
                        } catch (...) {
                            spdlog::warn(
                                "Retry attempt {} failed with unknown "
                                "exception.",
                                attempt + 1);
                            if (attempt == max_retries) {
                                throw;
                            }
                            if (backoff_ms.has_value()) {
                                std::this_thread::sleep_for(
                                    std::chrono::milliseconds(
                                        backoff_ms.value() * (attempt + 1)));
                            }
                        }
                        if (*sharedCancelled) {
                            spdlog::warn(
                                "Retry operation cancelled during attempt {}.",
                                attempt + 1);
                            THROW_INVALID_FUTURE_EXCEPTION(
                                "Future cancelled during retry");
                        }
                    }
                    spdlog::error("Retry failed after maximum attempts.");
                    THROW_INVALID_FUTURE_EXCEPTION(
                        "Retry failed after maximum attempts");
                })
                .share());
    }

    /**
     * @brief Checks if the future is ready.
     * @return True if the future is ready, false otherwise.
     */
    auto isReady() const noexcept -> bool {
        return future_.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    }

    /**
     * @brief Retrieves the result of the future.
     * @return The value of the future.
     * @throws InvalidFutureException if the future is cancelled.
     */
    auto get() -> T {
        if (*cancelled_) {
            spdlog::error("Attempted to get value from a cancelled future.");
            THROW_INVALID_FUTURE_EXCEPTION("Future has been cancelled");
        }
        return future_.get();
    }

    /**
     * @brief Promise type for coroutine support.
     */
    struct promise_type {
        std::promise<T> promise;

        /**
         * @brief Returns the EnhancedFuture associated with this promise.
         * @return An EnhancedFuture.
         */
        auto get_return_object() noexcept -> EnhancedFuture<T> {
            return EnhancedFuture<T>(promise.get_future().share());
        }

        /**
         * @brief Initial suspend point for the coroutine.
         * @return std::suspend_never to not suspend.
         */
        auto initial_suspend() noexcept -> std::suspend_never { return {}; }
        /**
         * @brief Final suspend point for the coroutine.
         * @return std::suspend_never to not suspend.
         */
        auto final_suspend() noexcept -> std::suspend_never { return {}; }

        /**
         * @brief Sets the return value of the coroutine.
         * @tparam U The type of the value.
         * @param value The value to set.
         */
        template <typename U>
            requires std::convertible_to<U, T>
        void return_value(U&& value) {
            promise.set_value(std::forward<U>(value));
        }

        /**
         * @brief Handles unhandled exceptions in the coroutine.
         */
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
    std::shared_ptr<std::mutex>
        callbacks_mutex_ptr_;  ///< Mutex for protecting callbacks_
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
    /**
     * @brief Promise type for coroutine support.
     */
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

#ifdef ATOM_USE_BOOST_LOCKFREE
    /**
     * @brief Callback wrapper for lockfree queue.
     */
    struct CallbackWrapper {
        std::function<void()> callback;

        CallbackWrapper() = default;
        explicit CallbackWrapper(std::function<void()> cb)
            : callback(std::move(cb)) {}
    };

    /**
     * @brief Lockfree callback container for void return type.
     */
    class LockfreeCallbackContainer {
    public:
        /**
         * @brief Constructs a LockfreeCallbackContainer.
         */
        LockfreeCallbackContainer() : queue_(128) {}  // Default capacity

        /**
         * @brief Adds a callback to the container.
         * @param callback The callback function.
         */
        void add(const std::function<void()>& callback) {
            auto* wrapper = new CallbackWrapper(callback);
            while (!queue_.push(wrapper)) {
                std::this_thread::yield();
            }
        }

        /**
         * @brief Executes all stored callbacks.
         */
        void executeAll() {
            CallbackWrapper* wrapper = nullptr;
            while (queue_.pop(wrapper)) {
                if (wrapper && wrapper->callback) {
                    try {
                        wrapper->callback();
                    } catch (const std::exception& e) {
                        spdlog::error(
                            "Exception in onComplete callback (void): {}",
                            e.what());
                    } catch (...) {
                        spdlog::error(
                            "Unknown exception in onComplete callback (void).");
                    }
                    delete wrapper;
                }
            }
        }

        /**
         * @brief Checks if the container is empty.
         * @return true if empty, false otherwise.
         */
        bool empty() const { return queue_.empty(); }

        /**
         * @brief Destroys the LockfreeCallbackContainer and cleans up remaining
         * wrappers.
         */
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
     * @brief Constructs an EnhancedFuture for void from a shared future.
     * @param fut The shared future to wrap.
     */
    explicit EnhancedFuture(std::shared_future<void>&& fut) noexcept
        : future_(std::move(fut)),
          cancelled_(std::make_shared<std::atomic<bool>>(false))
#ifdef ATOM_USE_BOOST_LOCKFREE
          ,
          callbacks_(std::make_shared<LockfreeCallbackContainer>())
#else
          ,  // Initialize callbacks_mutex_ptr_ here
          callbacks_mutex_ptr_(std::make_shared<std::mutex>()),
          callbacks_(std::make_shared<std::vector<std::function<void()>>>())
#endif
    {
    }

    /**
     * @brief Constructs an EnhancedFuture for void from a shared future.
     * @param fut The shared future to wrap.
     */
    explicit EnhancedFuture(const std::shared_future<void>& fut) noexcept
        : future_(fut),
          cancelled_(std::make_shared<std::atomic<bool>>(false))
#ifdef ATOM_USE_BOOST_LOCKFREE
          ,
          callbacks_(std::make_shared<LockfreeCallbackContainer>())
#else
          ,  // Initialize callbacks_mutex_ptr_ here
          callbacks_mutex_ptr_(std::make_shared<std::mutex>()),
          callbacks_(std::make_shared<std::vector<std::function<void()>>>())
#endif
    {
    }

    /**
     * @brief Move constructor.
     * @param other The other EnhancedFuture to move from.
     */
    EnhancedFuture(EnhancedFuture&& other) noexcept = default;
    /**
     * @brief Move assignment operator.
     * @param other The other EnhancedFuture to move from.
     * @return A reference to this EnhancedFuture.
     */
    EnhancedFuture& operator=(EnhancedFuture&& other) noexcept = default;
    /**
     * @brief Copy constructor.
     * @param other The other EnhancedFuture to copy from.
     */
    EnhancedFuture(const EnhancedFuture&) = default;
    /**
     * @brief Copy assignment operator.
     * @param other The other EnhancedFuture to copy from.
     * @return A reference to this EnhancedFuture.
     */
    EnhancedFuture& operator=(const EnhancedFuture&) = default;

    /**
     * @brief Chains another operation to be called after the void future is
     * done.
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
            std::async(
                std::launch::async,
                [sharedFuture, sharedCancelled,
                 func = std::forward<F>(func)]() -> ResultType {
                    if (*sharedCancelled) {
                        spdlog::warn(
                            "Then callback skipped: Future was cancelled.");
                        THROW_INVALID_FUTURE_EXCEPTION(
                            "Future has been cancelled");
                    }
                    if (sharedFuture->valid()) {
                        try {
                            sharedFuture->get();  // Wait for void future
                            return func();
                        } catch (const std::exception& e) {
                            spdlog::error(
                                "Exception in then callback (void): {}",
                                e.what());
                            THROW_INVALID_FUTURE_EXCEPTION(
                                "Exception in then callback");
                        } catch (...) {
                            spdlog::error(
                                "Unknown exception in then callback (void).");
                            THROW_INVALID_FUTURE_EXCEPTION(
                                "Unknown exception in then callback");
                        }
                    }
                    spdlog::error("Then callback failed: Future is invalid.");
                    THROW_INVALID_FUTURE_EXCEPTION("Future is invalid");
                })
                .share());
    }

    /**
     * @brief Waits for the void future with a timeout.
     * @param timeout The timeout duration.
     * @return true if the future completed within the timeout, false otherwise.
     */
    auto waitFor(std::chrono::milliseconds timeout) noexcept -> bool {
        if (future_.wait_for(timeout) == std::future_status::ready &&
            !*cancelled_) {
            try {
                future_.get();
                return true;
            } catch (const std::exception& e) {
                spdlog::error("Exception during waitFor get (void): {}",
                              e.what());
                return false;
            } catch (...) {
                spdlog::error("Unknown exception during waitFor get (void).");
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
     * @brief Sets a completion callback to be called when the void future is
     * done.
     * @tparam F The type of the callback function.
     * @param func The callback function to add.
     */
    template <ValidCallable F>
    void onComplete(F&& func) {
        if (*cancelled_) {
            spdlog::warn(
                "onComplete callback not added: Future already cancelled.");
            return;
        }

#ifdef ATOM_USE_BOOST_LOCKFREE
        callbacks_->add(std::function<void()>(std::forward<F>(func)));
#else
        {
            std::lock_guard<std::mutex> lock(*callbacks_mutex_ptr_);
            callbacks_->emplace_back(std::forward<F>(func));
        }
#endif

#ifdef ATOM_USE_ASIO
        asio::post(
            atom::async::internal::get_asio_thread_pool(),
            [future = future_, callbacks = callbacks_, cancelled = cancelled_
#ifndef ATOM_USE_BOOST_LOCKFREE
             ,
             callbacks_mutex_ptr = callbacks_mutex_ptr_
#endif
        ]() mutable {
                try {
                    if (!*cancelled && future.valid()) {
                        future.get();  // Wait for void future
                        if (!*cancelled) {
#ifdef ATOM_USE_BOOST_LOCKFREE
                            callbacks->executeAll();
#else
                            std::lock_guard<std::mutex> lock(
                                *callbacks_mutex_ptr);
                            for (auto& callback_fn : *callbacks) {
                                try {
                                    callback_fn();
                                } catch (const std::exception& e) {
                                    spdlog::error(
                                        "Exception in onComplete callback "
                                        "(void, vector): {}",
                                        e.what());
                                } catch (...) {
                                    spdlog::error(
                                        "Unknown exception in onComplete "
                                        "callback (void, vector).");
                                }
                            }
#endif
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::warn(
                        "Future completed with exception in onComplete handler "
                        "(void): {}",
                        e.what());
                } catch (...) {
                    spdlog::warn(
                        "Future completed with unknown exception in onComplete "
                        "handler (void).");
                }
            });
#else  // Original std::thread implementation
        std::thread([future = future_, callbacks = callbacks_,
                     cancelled = cancelled_
#ifndef ATOM_USE_BOOST_LOCKFREE
                     ,
                     callbacks_mutex_ptr = callbacks_mutex_ptr_
#endif
        ]() mutable {
            try {
                if (!*cancelled && future.valid()) {
                    future.get();
                    if (!*cancelled) {
#ifdef ATOM_USE_BOOST_LOCKFREE
                        callbacks->executeAll();
#else
                        std::lock_guard<std::mutex> lock(*callbacks_mutex_ptr);
                        for (auto& callback : *callbacks) {
                            try {
                                callback();
                            } catch (const std::exception& e) {
                                spdlog::error(
                                    "Exception in onComplete callback (void, "
                                    "vector): {}",
                                    e.what());
                            } catch (...) {
                                spdlog::error(
                                    "Unknown exception in onComplete callback "
                                    "(void, vector).");
                            }
                        }
#endif
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn(
                    "Future completed with exception in onComplete handler "
                    "(void): {}",
                    e.what());
            } catch (...) {
                spdlog::warn(
                    "Future completed with unknown exception in onComplete "
                    "handler (void).");
            }
        }).detach();
#endif
    }

    /**
     * @brief Waits synchronously for the void future to complete.
     * @throws InvalidFutureException if the future is cancelled or an exception
     * occurs.
     */
    void wait() {
        if (*cancelled_) {
            spdlog::error("Attempted to wait on a cancelled void future.");
            THROW_INVALID_FUTURE_EXCEPTION("Future has been cancelled");
        }
        try {
            future_.get();
        } catch (const std::exception& e) {
            spdlog::error("Exception while waiting for void future: {}",
                          e.what());
            THROW_INVALID_FUTURE_EXCEPTION(
                "Exception while waiting for future: ", e.what());
        } catch (...) {
            spdlog::error("Unknown exception while waiting for void future.");
            THROW_INVALID_FUTURE_EXCEPTION(
                "Unknown exception while waiting for future");
        }
    }

    /**
     * @brief Cancels the void future.
     */
    void cancel() noexcept {
        if (!*cancelled_) {
            *cancelled_ = true;
            spdlog::debug("Void future cancelled.");
        }
    }
    /**
     * @brief Checks if the void future has been cancelled.
     * @return True if the future has been cancelled, false otherwise.
     */
    [[nodiscard]] auto isCancelled() const noexcept -> bool {
        return *cancelled_;
    }

    /**
     * @brief Gets the exception associated with the void future, if any.
     * @return A pointer to the exception, or nullptr if no exception.
     */
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

    /**
     * @brief Checks if the void future is ready.
     * @return True if the future is ready, false otherwise.
     */
    auto isReady() const noexcept -> bool {
        return future_.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::ready;
    }

    /**
     * @brief Retrieves the result of the void future (waits for completion).
     * @throws InvalidFutureException if the future is cancelled.
     */
    void get() {
        if (*cancelled_) {
            spdlog::error(
                "Attempted to get value from a cancelled void future.");
            THROW_INVALID_FUTURE_EXCEPTION("Future has been cancelled");
        }
        future_.get();
    }

    /**
     * @brief Promise type for coroutine support.
     */
    struct promise_type {
        std::promise<void> promise;
        /**
         * @brief Returns the EnhancedFuture associated with this promise.
         * @return An EnhancedFuture.
         */
        auto get_return_object() noexcept -> EnhancedFuture<void> {
            return EnhancedFuture<void>(promise.get_future().share());
        }
        /**
         * @brief Initial suspend point for the coroutine.
         * @return std::suspend_never to not suspend.
         */
        auto initial_suspend() noexcept -> std::suspend_never { return {}; }
        /**
         * @brief Final suspend point for the coroutine.
         * @return std::suspend_never to not suspend.
         */
        auto final_suspend() noexcept -> std::suspend_never { return {}; }
        /**
         * @brief Sets the return value of the coroutine (void).
         */
        void return_void() noexcept { promise.set_value(); }
        /**
         * @brief Handles unhandled exceptions in the coroutine.
         */
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
    std::shared_ptr<std::mutex> callbacks_mutex_ptr_;
    std::shared_ptr<std::vector<std::function<void()>>> callbacks_;
#endif
};

/**
 * @brief Create a thread pool optimized EnhancedFuture.
 * @tparam F Function type.
 * @tparam Args Parameter types.
 * @param f Function to be called.
 * @param args Parameters to pass to the function.
 * @return EnhancedFuture of the function result.
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
            } catch (const std::exception& e) {
                spdlog::error("Exception in Asio task: {}", e.what());
                p.set_exception(std::current_exception());
            } catch (...) {
                spdlog::error("Unknown exception in Asio task.");
                p.set_exception(std::current_exception());
            }
        });
    return EnhancedFuture<result_type>(future.share());

#elif defined(ATOM_PLATFORM_MACOS) && !defined(ATOM_USE_ASIO)
    std::promise<result_type> promise;
    auto future = promise.get_future();

    struct CallData {
        std::promise<result_type> promise;
        std::function<void()> work;

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
                } catch (const std::exception& e) {
                    spdlog::error("Exception in macOS dispatch task: {}",
                                  e.what());
                    this->promise.set_exception(std::current_exception());
                } catch (...) {
                    spdlog::error("Unknown exception in macOS dispatch task.");
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
        std::remove_cvref_t<decltype(std::declval<typename std::iterator_traits<InputIt>::value_type>().get())>>> {
    using EnhancedFutureType =
        typename std::iterator_traits<InputIt>::value_type;
    using ValueType = std::remove_cvref_t<decltype(std::declval<EnhancedFutureType>().get())>;
    using ResultType = std::vector<ValueType>;

    if (std::distance(first, last) < 0) {
        spdlog::error("Invalid iterator range provided to whenAll.");
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
                    auto status = fut.wait_for(timeout.value());
                    if (status == std::future_status::timeout) {
                        if (!promise_fulfilled->exchange(true)) {
                            spdlog::warn(
                                "whenAll: Timeout while waiting for future "
                                "{} of {}.",
                                i + 1, total_count);
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
                        } else {
                            // This case should ideally not be reached if
                            // fut.get() succeeded and ValueType is not void.
                            // Log an error if it does.
                            spdlog::error(
                                "whenAll: Non-void future result missing for "
                                "index {}.",
                                i);
                        }
                    }
                }
                promise_ptr->set_value(std::move(*results_ptr));
            }
        } catch (const std::exception& e) {
            if (!promise_fulfilled->exchange(true)) {
                spdlog::error("Exception in whenAll: {}", e.what());
                promise_ptr->set_exception(std::current_exception());
            }
        } catch (...) {
            if (!promise_fulfilled->exchange(true)) {
                spdlog::error("Unknown exception in whenAll.");
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
    requires(FutureCompatible<future_value_t<std::decay_t<Futures>>> && ...)
auto whenAll(Futures&&... futures)
    -> std::future<std::tuple<future_value_t<std::decay_t<Futures>>...>> {
    auto promise = std::make_shared<
        std::promise<std::tuple<future_value_t<std::decay_t<Futures>>...>>>();
    std::future<std::tuple<future_value_t<std::decay_t<Futures>>...>>
        resultFuture = promise->get_future();

    auto futuresTuple = std::make_shared<std::tuple<std::decay_t<Futures>...>>(
        std::forward<Futures>(futures)...);

    std::thread([promise, futuresTuple]() mutable {
        try {
            auto results = std::apply(
                [](auto&... fs) { return std::make_tuple(fs.get()...); },
                *futuresTuple);
            promise->set_value(std::move(results));
        } catch (const std::exception& e) {
            spdlog::error("Exception in whenAll (variadic): {}", e.what());
            promise->set_exception(std::current_exception());
        } catch (...) {
            spdlog::error("Unknown exception in whenAll (variadic).");
            promise->set_exception(std::current_exception());
        }
    }).detach();

    return resultFuture;
}

/**
 * @brief Helper function to create a coroutine-based EnhancedFuture.
 * @tparam T The type of the value.
 * @param value The value to return.
 * @return An EnhancedFuture.
 */
template <FutureCompatible T>
EnhancedFuture<T> co_makeEnhancedFuture(T value) {
    co_return value;
}

/**
 * @brief Specialization for void to create a coroutine-based EnhancedFuture.
 * @return An EnhancedFuture<void>.
 */
inline EnhancedFuture<void> co_makeEnhancedFuture() { co_return; }

/**
 * @brief Utility to run parallel operations on a data collection.
 * @tparam Range The type of the input range.
 * @tparam Func The type of the function to apply.
 * @param range The input range.
 * @param func The function to apply to each element.
 * @param numTasks The number of parallel tasks to create. If 0, determined
 * automatically.
 * @return A vector of EnhancedFutures, each representing a chunk of processed
 * data.
 */
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
            numTasks = 2;  // Fallback if hardware_concurrency is 0
            spdlog::warn(
                "Could not determine hardware concurrency, defaulting to {} "
                "parallel tasks.",
                numTasks);
        }
    }

    std::vector<EnhancedFuture<TaskChunkResultType>> futures;
    auto begin = std::ranges::begin(range);
    auto end = std::ranges::end(range);
    size_t totalSize = static_cast<size_t>(std::ranges::distance(range));

    if (totalSize == 0) {
        spdlog::debug(
            "parallelProcess: Empty range provided, returning empty futures "
            "vector.");
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
    spdlog::debug("parallelProcess: Created {} futures for {} items.",
                  futures.size(), totalSize);
    return futures;
}



}  // namespace atom::async

#endif  // ATOM_ASYNC_FUTURE_HPP

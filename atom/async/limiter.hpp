#ifndef ATOM_ASYNC_LIMITER_HPP
#define ATOM_ASYNC_LIMITER_HPP

#include <atomic>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <deque>
#include <format>
#include <ranges>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>

// Platform-specific includes
#if defined(_WIN32) || defined(_WIN64)
#define ATOM_PLATFORM_WINDOWS
#include <windows.h>
#elif defined(__APPLE__)
#define ATOM_PLATFORM_MACOS
#include <dispatch/dispatch.h>
#elif defined(__linux__)
#define ATOM_PLATFORM_LINUX
#include <semaphore.h>
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

#ifdef ATOM_USE_ASIO
#include <asio/post.hpp>
#include <asio/thread_pool.hpp>
// Assuming atom::async::internal::get_asio_thread_pool() is available
// from "atom/async/future.hpp" or a similar common header.
#include "atom/async/future.hpp"  // Ensure this provides get_asio_thread_pool
#endif

namespace atom::async {

/**
 * @brief Custom exception type using source_location for better error tracking.
 */
class RateLimitExceededException : public std::runtime_error {
public:
    explicit RateLimitExceededException(
        const std::string& message,
        std::source_location location = std::source_location::current())
        : std::runtime_error(
              std::format("Rate limit exceeded at {}:{} in function {}: {}",
                          location.file_name(), location.line(),
                          location.function_name(), message)) {}
};

/**
 * @brief Concept for a callable object that takes no arguments and returns
 * void.
 */
template <typename F>
concept Callable =
    std::invocable<F> && std::same_as<std::invoke_result_t<F>, void>;

/**
 * @brief Concept for a callable object that can be cancelled.
 */
template <typename F>
concept CancellableCallable = Callable<F> && requires(F f) {
    { f.cancel() } -> std::same_as<void>;
};

/**
 * @brief A rate limiter class to control the rate of function executions.
 */
class RateLimiter {
public:
    /**
     * @brief Settings for the rate limiter.
     */
    struct Settings {
        size_t maxRequests;  ///< Maximum number of requests allowed in the time
                             ///< window.
        std::chrono::seconds
            timeWindow;  ///< The time window in which maxRequests are allowed.

        /**
         * @brief Constructor for Settings with validation.
         * @param max_requests Maximum number of requests.
         * @param time_window Duration of the time window.
         * @throws std::invalid_argument if parameters are invalid (e.g.,
         * max_requests is 0 or time_window is zero/negative).
         */
        explicit Settings(
            size_t max_requests = 5,
            std::chrono::seconds time_window = std::chrono::seconds(1))
            : maxRequests(max_requests), timeWindow(time_window) {
            if (maxRequests == 0) {
                throw std::invalid_argument(
                    "maxRequests must be greater than 0.");
            }
            if (timeWindow <= std::chrono::seconds(0)) {
                throw std::invalid_argument(
                    "timeWindow must be a positive duration.");
            }
        }
    };

    /**
     * @brief Constructor for RateLimiter.
     */
    RateLimiter() noexcept;

    /**
     * @brief Destructor.
     */
    ~RateLimiter() noexcept;

    // Move constructor and assignment operator
    RateLimiter(RateLimiter&&) noexcept;
    RateLimiter& operator=(RateLimiter&&) noexcept;

    // Copy operations are deleted
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    /**
     * @brief Awaiter class for handling coroutines.
     */
    class [[nodiscard]] Awaiter {
    public:
        /**
         * @brief Constructor for Awaiter.
         * @param limiter Reference to the rate limiter.
         * @param function_name Name of the function to be rate-limited.
         */
        Awaiter(RateLimiter& limiter, std::string function_name) noexcept;

        /**
         * @brief Checks if the awaiter is ready.
         * @return Always returns false to suspend.
         */
        [[nodiscard]] auto await_ready() const noexcept -> bool;

        /**
         * @brief Suspends the coroutine and enqueues it.
         * @param handle Coroutine handle.
         */
        void await_suspend(std::coroutine_handle<> handle);

        /**
         * @brief Resumes the coroutine.
         * @throws RateLimitExceededException if rate limit was exceeded (though
         * this is typically checked before allowing resumption).
         */
        void await_resume();

    private:
        friend class RateLimiter;  // Allow RateLimiter to set was_rejected_
        RateLimiter& limiter_;
        std::string function_name_;
        bool was_rejected_ = false;
    };

    /**
     * @brief Acquires the rate limiter for a specific function.
     * @param function_name Name of the function to be rate-limited.
     * @return An Awaiter object.
     */
    [[nodiscard]] Awaiter acquire(std::string_view function_name);

    /**
     * @brief Acquires rate limiters in batch for multiple functions (C++20
     * range).
     * @param function_names A range of function names.
     * @return A vector of Awaiter objects.
     */
    template <std::ranges::range R>
        requires std::convertible_to<std::ranges::range_value_t<R>,
                                     std::string_view>
    [[nodiscard]] auto acquireBatch(R&& function_names) {
        std::vector<Awaiter> awaiters;
        if constexpr (std::ranges::sized_range<R>) {
            awaiters.reserve(std::ranges::size(function_names));
        }

        for (const auto& name : function_names) {
            awaiters.emplace_back(
                *this, std::string(name));  // Pass *this for the limiter
        }
        // Note: The awaiters are returned, but actual acquisition/suspension
        // happens when co_await is used on them.
        return awaiters;
    }

    /**
     * @brief Sets the rate limit for a specific function.
     * @param function_name Name of the function to be rate-limited.
     * @param max_requests Maximum number of requests allowed.
     * @param time_window Duration of the time window.
     * @throws std::invalid_argument if parameters are invalid.
     */
    void setFunctionLimit(std::string_view function_name, size_t max_requests,
                          std::chrono::seconds time_window);

    /**
     * @brief Sets rate limits for multiple functions in batch using C++20 span.
     * @param settings A span of pairs, where each pair contains a function name
     * and its rate limit settings.
     */
    void setFunctionLimits(
        std::span<const std::pair<std::string_view, Settings>> settings_list);

    /**
     * @brief Pauses the rate limiter. No new requests will be processed until
     * resumed.
     */
    void pause() noexcept;

    /**
     * @brief Resumes the rate limiter and processes any pending requests.
     */
    void resume();

    /**
     * @brief Prints the log of requests (for debugging).
     */
    void printLog() const noexcept;

    /**
     * @brief Gets the number of rejected requests for a specific function.
     * @param function_name Name of the function.
     * @return Number of rejected requests.
     */
    [[nodiscard]] auto getRejectedRequests(
        std::string_view function_name) const noexcept -> size_t;

    /**
     * @brief Resets the rate limit counter and rejected count for a specific
     * function.
     * @param function_name The name of the function.
     */
    void resetFunction(std::string_view function_name);

    /**
     * @brief Resets all rate limit counters and rejected counts.
     */
    void resetAll() noexcept;

    /**
     * @brief Processes waiting coroutines. Public for manual triggering if not
     * using automated mechanisms.
     */
    void processWaiters();

#if !defined(TEST_F) && \
    !defined(TEST)  // For testing purposes, members might be public
private:
#endif
    void cleanup(std::string_view function_name,
                 const std::chrono::seconds& time_window);
    void
    triggerProcessingMechanism();  // Internal helper to signal/post processing

#ifdef ATOM_USE_ASIO
    std::atomic<bool> processing_posted_{false};
    void postProcessWaitersTask();
#else
// Platform-specific synchronization primitives
#if defined(ATOM_PLATFORM_WINDOWS)
    CONDITION_VARIABLE resumeCondition_{};
    CRITICAL_SECTION resumeLock_{};
    // A dedicated thread would typically wait on resumeCondition_ and call
    // processWaiters. Or, optimizedProcessWaiters (if defined) would be this
    // logic.
#elif defined(ATOM_PLATFORM_MACOS)
    dispatch_semaphore_t resume_semaphore_gcd_{nullptr};
    // A GCD block would wait on resume_semaphore_gcd_ and dispatch
    // processWaiters.
#elif defined(ATOM_PLATFORM_LINUX)
    sem_t resumeSemaphore_{};
    // A dedicated thread would typically wait on resumeSemaphore_ and call
    // processWaiters.
#endif
    // The optimizedProcessWaiters methods declared below are assumed to be part
    // of the non-Asio processing mechanism if they were fully implemented.
#endif

// These seem to be intended as platform-specific ways to run processWaiters.
// If ATOM_USE_ASIO is defined, these are not strictly needed as Asio handles
// the execution context.
#ifndef ATOM_USE_ASIO
#if defined(ATOM_PLATFORM_WINDOWS)
    void optimizedProcessWaiters();  // Example: Waits on resumeCondition_ then
                                     // calls processWaiters()
#elif defined(ATOM_PLATFORM_MACOS)
    void optimizedProcessWaiters();  // Example: Waits on resume_semaphore_gcd_
                                     // then calls processWaiters()
#elif defined(ATOM_PLATFORM_LINUX)
    void optimizedProcessWaiters();  // Example: Waits on resumeSemaphore_ then
                                     // calls processWaiters()
#endif
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
    struct RequestEntry {
        std::chrono::steady_clock::time_point timestamp;
        RequestEntry() = default;
        explicit RequestEntry(std::chrono::steady_clock::time_point time)
            : timestamp(time) {}
    };
    struct WaiterEntry {
        std::coroutine_handle<> handle;
        std::string function_name_ref;  // To know which function this waiter
                                        // belongs to for Awaiter::was_rejected_
        Awaiter* awaiter_ptr;  // Pointer to the awaiter to set was_rejected_

        WaiterEntry() = default;
        explicit WaiterEntry(std::coroutine_handle<> h, std::string name_ref,
                             Awaiter* apt)
            : handle(h),
              function_name_ref(std::move(name_ref)),
              awaiter_ptr(apt) {}
    };
    class LockfreeRequestQueue {
    public:
        LockfreeRequestQueue() : m_queue(128) {}  // Default capacity

        void push(const std::chrono::steady_clock::time_point& time) {
            RequestEntry entry(time);
            while (!m_queue.push(entry)) {
                std::this_thread::yield();
            }
        }
        bool pop(std::chrono::steady_clock::time_point& time) {
            RequestEntry entry;
            if (m_queue.pop(entry)) {
                time = entry.timestamp;
                return true;
            }
            return false;
        }
        bool empty() const { return m_queue.empty(); }
        void clear() {
            RequestEntry entry;
            while (m_queue.pop(entry)) {
            }
        }
        size_t size_approx() const { return m_queue.read_available(); }

    private:
        boost::lockfree::queue<RequestEntry> m_queue;
    };
    class LockfreeWaiterQueue {
    public:
        LockfreeWaiterQueue() : m_queue(128) {}  // Default capacity

        void push(std::coroutine_handle<> handle, std::string func_name,
                  Awaiter* apt) {
            WaiterEntry entry(handle, std::move(func_name), apt);
            while (!m_queue.push(entry)) {
                std::this_thread::yield();
            }
        }
        bool pop(std::coroutine_handle<>& handle, std::string& func_name,
                 Awaiter*& apt) {
            WaiterEntry entry;
            if (m_queue.pop(entry)) {
                handle = entry.handle;
                func_name = std::move(entry.function_name_ref);
                apt = entry.awaiter_ptr;
                return true;
            }
            return false;
        }
        bool empty() const { return m_queue.empty(); }
        size_t size_approx() const { return m_queue.read_available(); }

    private:
        boost::lockfree::queue<WaiterEntry> m_queue;
    };

    std::unordered_map<std::string, Settings> settings_;
    std::unordered_map<std::string, LockfreeRequestQueue> requests_;
    std::unordered_map<std::string, LockfreeWaiterQueue>
        waiters_;  // Keyed by function name
    
    // Atomic counter for waiters ready to be processed
    std::atomic<int> waitersReady_{0};
    
    // Thread-safe queue for waiters that are ready to be processed
    class GlobalWaiterQueue {
    public:
        GlobalWaiterQueue() : m_queue(256) {}
        
        void push(std::coroutine_handle<> handle, std::string func_name, Awaiter* apt) {
            WaiterEntry entry(handle, std::move(func_name), apt);
            while (!m_queue.push(entry)) {
                std::this_thread::yield();
            }
        }
        
        bool pop(std::coroutine_handle<>& handle, std::string& func_name, Awaiter*& apt) {
            WaiterEntry entry;
            if (m_queue.pop(entry)) {
                handle = entry.handle;
                func_name = std::move(entry.function_name_ref);
                apt = entry.awaiter_ptr;
                return true;
            }
            return false;
        }
        
        bool empty() const { return m_queue.empty(); }
        size_t size_approx() const { return m_queue.read_available(); }
        
    private:
        boost::lockfree::queue<WaiterEntry> m_queue;
    };
    
    // Global queue for ready waiters
    GlobalWaiterQueue globalWaiters_;
    
    // Processer thread
    std::jthread processorThread_;
    std::atomic<bool> running_{true};
    
    // SPSC queue for fast inter-thread signaling
    boost::lockfree::spsc_queue<char> signalQueue_{1024};
#else
    struct WaiterInfo {
        std::coroutine_handle<> handle;
        Awaiter* awaiter_ptr;  // To set was_rejected_

        WaiterInfo(std::coroutine_handle<> h, Awaiter* apt)
            : handle(h), awaiter_ptr(apt) {}
    };
    std::unordered_map<std::string, Settings> settings_;
    std::unordered_map<std::string,
                       std::deque<std::chrono::steady_clock::time_point>>
        requests_;
    std::unordered_map<std::string, std::deque<WaiterInfo>>
        waiters_;  // Keyed by function name
#endif

    std::unordered_map<std::string,
                       std::deque<std::chrono::steady_clock::time_point>>
        log_;
    std::unordered_map<std::string, std::atomic<size_t>> rejected_requests_;
    std::atomic<bool> paused_ = false;
    mutable std::shared_mutex
        mutex_;  // Using a read-write lock for better concurrent performance
};


/**
 * @class RateLimiterSingleton
 * @brief Singleton rate limiter, providing a global access point.
 */
class RateLimiterSingleton {
public:
    /**
     * @brief Gets the singleton instance.
     * @return Reference to the RateLimiter.
     */
    static RateLimiter& instance() {
        static RateLimiter limiter_instance;  // Meyers' Singleton
        return limiter_instance;
    }

    // Construction and copying are prohibited
    RateLimiterSingleton() = delete;
    RateLimiterSingleton(const RateLimiterSingleton&) = delete;
    RateLimiterSingleton& operator=(const RateLimiterSingleton&) = delete;
    RateLimiterSingleton(RateLimiterSingleton&&) = delete;
    RateLimiterSingleton& operator=(RateLimiterSingleton&&) = delete;
};


}  // namespace atom::async

#endif  // ATOM_ASYNC_LIMITER_HPP

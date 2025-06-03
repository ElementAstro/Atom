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
#include "atom/async/future.hpp"
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
 * @brief A high-performance rate limiter class to control the rate of function
 * executions.
 */
class RateLimiter {
public:
    /**
     * @brief Settings for the rate limiter with validation.
     */
    struct Settings {
        size_t maxRequests;
        std::chrono::seconds timeWindow;

        /**
         * @brief Constructor for Settings with validation.
         * @param max_requests Maximum number of requests allowed in the time
         * window.
         * @param time_window Duration of the time window.
         * @throws std::invalid_argument if parameters are invalid.
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
     * @brief Default constructor for RateLimiter.
     */
    RateLimiter() noexcept;

    /**
     * @brief Destructor that properly cleans up resources.
     */
    ~RateLimiter() noexcept;

    RateLimiter(RateLimiter&&) noexcept;
    RateLimiter& operator=(RateLimiter&&) noexcept;

    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    /**
     * @brief Awaiter class for handling coroutines with optimized suspension.
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
         * @return Always returns false to suspend and check rate limit.
         */
        [[nodiscard]] auto await_ready() const noexcept -> bool;

        /**
         * @brief Suspends the coroutine and enqueues it for rate limiting.
         * @param handle Coroutine handle to suspend.
         */
        void await_suspend(std::coroutine_handle<> handle);

        /**
         * @brief Resumes the coroutine after rate limit check.
         * @throws RateLimitExceededException if rate limit was exceeded.
         */
        void await_resume();

    private:
        friend class RateLimiter;
        RateLimiter& limiter_;
        std::string function_name_;
        bool was_rejected_ = false;
    };

    /**
     * @brief Acquires the rate limiter for a specific function.
     * @param function_name Name of the function to be rate-limited.
     * @return An Awaiter object for coroutine suspension.
     */
    [[nodiscard]] Awaiter acquire(std::string_view function_name);

    /**
     * @brief Acquires rate limiters in batch for multiple functions.
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
            awaiters.emplace_back(*this, std::string(name));
        }
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
     * @brief Sets rate limits for multiple functions in batch.
     * @param settings_list A span of pairs containing function names and their
     * settings.
     */
    void setFunctionLimits(
        std::span<const std::pair<std::string_view, Settings>> settings_list);

    /**
     * @brief Pauses the rate limiter, preventing new request processing.
     */
    void pause() noexcept;

    /**
     * @brief Resumes the rate limiter and processes pending requests.
     */
    void resume();

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
     * @param function_name The name of the function to reset.
     */
    void resetFunction(std::string_view function_name);

    /**
     * @brief Resets all rate limit counters and rejected counts.
     */
    void resetAll() noexcept;

    /**
     * @brief Processes waiting coroutines manually.
     */
    void processWaiters();

private:
    void cleanup(std::string_view function_name,
                 const std::chrono::seconds& time_window);

#ifdef ATOM_USE_ASIO
    void asioProcessWaiters();
    mutable asio::thread_pool asio_pool_;
#endif

#ifdef ATOM_PLATFORM_WINDOWS
    void optimizedProcessWaiters();
    CONDITION_VARIABLE resumeCondition_{};
    CRITICAL_SECTION resumeLock_{};
#elif defined(ATOM_PLATFORM_MACOS)
    void optimizedProcessWaiters();
#elif defined(ATOM_PLATFORM_LINUX)
    void optimizedProcessWaiters();
    sem_t resumeSemaphore_{};
    std::atomic<int> waitersReady_{0};
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
    using LockfreeRequestQueue =
        boost::lockfree::queue<std::chrono::steady_clock::time_point>;
    using LockfreeWaiterQueue = boost::lockfree::queue<std::coroutine_handle<>>;

    std::unordered_map<std::string, LockfreeRequestQueue> requests_;
    std::unordered_map<std::string, LockfreeWaiterQueue> waiters_;
#else
    struct WaiterInfo {
        std::coroutine_handle<> handle;
        Awaiter* awaiter_ptr;

        WaiterInfo(std::coroutine_handle<> h, Awaiter* apt)
            : handle(h), awaiter_ptr(apt) {}
    };

    std::unordered_map<std::string,
                       std::deque<std::chrono::steady_clock::time_point>>
        requests_;
    std::unordered_map<std::string, std::deque<WaiterInfo>> waiters_;
#endif

    std::unordered_map<std::string, Settings> settings_;
    std::unordered_map<std::string, std::atomic<size_t>> rejected_requests_;
    std::atomic<bool> paused_ = false;
    mutable std::shared_mutex mutex_;
};

/**
 * @brief Singleton rate limiter providing global access point.
 */
class RateLimiterSingleton {
public:
    /**
     * @brief Gets the singleton instance using Meyer's singleton pattern.
     * @return Reference to the global RateLimiter instance.
     */
    static RateLimiter& instance() {
        static RateLimiter limiter_instance;
        return limiter_instance;
    }

    RateLimiterSingleton() = delete;
    RateLimiterSingleton(const RateLimiterSingleton&) = delete;
    RateLimiterSingleton& operator=(const RateLimiterSingleton&) = delete;
    RateLimiterSingleton(RateLimiterSingleton&&) = delete;
    RateLimiterSingleton& operator=(RateLimiterSingleton&&) = delete;
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_LIMITER_HPP

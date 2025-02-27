#ifndef ATOM_ASYNC_LIMITER_HPP
#define ATOM_ASYNC_LIMITER_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <deque>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace atom::async {

/**
 * 自定义异常类型
 */
class RateLimitExceededException : public std::runtime_error {
public:
    explicit RateLimitExceededException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * 函数概念：定义可调用对象
 */
template <typename F>
concept Callable = requires(F f) {
    { f() } -> std::same_as<void>;
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
         * @throws std::invalid_argument if parameters are invalid
         */
        explicit Settings(
            size_t max_requests = 5,
            std::chrono::seconds time_window = std::chrono::seconds(1));
    };

    /**
     * @brief Constructor for RateLimiter.
     */
    RateLimiter() noexcept;

    /**
     * @brief Destructor
     */
    ~RateLimiter() noexcept;

    // 移动构造和赋值函数
    RateLimiter(RateLimiter&&) noexcept;
    RateLimiter& operator=(RateLimiter&&) noexcept;

    // 禁止复制
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
         * @return Always returns false.
         */
        [[nodiscard]] auto await_ready() const noexcept -> bool;

        /**
         * @brief Suspends the coroutine.
         * @param handle Coroutine handle.
         */
        void await_suspend(std::coroutine_handle<> handle);

        /**
         * @brief Resumes the coroutine.
         * @throws RateLimitExceededException if rate limit was exceeded
         */
        void await_resume();

    private:
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
     * @brief Sets the rate limit for a specific function.
     * @param function_name Name of the function to be rate-limited.
     * @param max_requests Maximum number of requests allowed.
     * @param time_window Duration of the time window.
     * @throws std::invalid_argument if parameters are invalid
     */
    void setFunctionLimit(std::string_view function_name, size_t max_requests,
                          std::chrono::seconds time_window);

    /**
     * @brief Pauses the rate limiter.
     */
    void pause() noexcept;

    /**
     * @brief Resumes the rate limiter.
     */
    void resume();

    /**
     * @brief Prints the log of requests.
     */
    void printLog() const noexcept;

    /**
     * @brief Gets the number of rejected requests for a specific function.
     * @param function_name Name of the function.
     * @return Number of rejected requests.
     */
    [[nodiscard]] auto getRejectedRequests(
        std::string_view function_name) const noexcept -> size_t;

#if !defined(TEST_F) && !defined(TEST)
private:
#endif
    /**
     * @brief Cleans up old requests outside the time window.
     * @param function_name Name of the function.
     * @param time_window Duration of the time window.
     */
    void cleanup(std::string_view function_name,
                 const std::chrono::seconds& time_window);

    /**
     * @brief Processes waiting coroutines.
     */
    void processWaiters();

    std::unordered_map<std::string, Settings> settings_;
    std::unordered_map<std::string,
                       std::deque<std::chrono::steady_clock::time_point>>
        requests_;
    std::unordered_map<std::string, std::deque<std::coroutine_handle<>>>
        waiters_;
    std::unordered_map<std::string,
                       std::deque<std::chrono::steady_clock::time_point>>
        log_;
    std::unordered_map<std::string, std::atomic<size_t>> rejected_requests_;
    std::atomic<bool> paused_ = false;
    mutable std::shared_mutex mutex_;  // 使用读写锁提高并发性能
};

/**
 * @class Debounce
 * @brief A class that implements a debouncing mechanism for function calls.
 */
template <Callable F>
class Debounce {
public:
    /**
     * @brief Constructs a Debounce object.
     *
     * @param func The function to be debounced.
     * @param delay The time delay to wait before invoking the function.
     * @param leading If true, the function will be invoked immediately on the
     * first call and then debounced for subsequent calls. If false, the
     * function will be debounced and invoked only after the delay has passed
     * since the last call.
     * @param maxWait Optional maximum wait time before invoking the function if
     * it has been called frequently. If not provided, there is no maximum wait
     * time.
     * @throws std::invalid_argument if delay is negative
     */
    explicit Debounce(
        F func, std::chrono::milliseconds delay, bool leading = false,
        std::optional<std::chrono::milliseconds> maxWait = std::nullopt);

    /**
     * @brief Invokes the debounced function if the delay has elapsed since the
     * last call.
     */
    void operator()() noexcept;

    /**
     * @brief Cancels any pending function calls.
     */
    void cancel() noexcept;

    /**
     * @brief Immediately invokes the function if it is scheduled to be called.
     */
    void flush() noexcept;

    /**
     * @brief Resets the debouncer, clearing any pending function call and
     * timer.
     */
    void reset() noexcept;

    /**
     * @brief Returns the number of times the function has been invoked.
     * @return The count of function invocations.
     */
    [[nodiscard]] size_t callCount() const noexcept;

private:
    /**
     * @brief Runs the function in a separate thread after the debounce delay.
     */
    void run();

    F func_;  ///< The function to be debounced.
    std::chrono::milliseconds
        delay_;  ///< The time delay before invoking the function.
    std::optional<std::chrono::steady_clock::time_point>
        last_call_;        ///< The timestamp of the last call.
    std::jthread thread_;  ///< A thread used to handle delayed function calls.
    mutable std::mutex
        mutex_;     ///< Mutex to protect concurrent access to internal state.
    bool leading_;  ///< Indicates if the function should be called immediately
                    ///< upon the first call.
    std::atomic<bool> scheduled_ =
        false;  ///< Flag to track if the function is scheduled for execution.
    std::optional<std::chrono::milliseconds>
        maxWait_;  ///< Optional maximum wait time before invocation.
    std::atomic<size_t> call_count_{
        0};  ///< Counter to keep track of function call invocations.
};

/**
 * @class Throttle
 * @brief A class that provides throttling for function calls, ensuring they are
 * not invoked more frequently than a specified interval.
 */
template <Callable F>
class Throttle {
public:
    /**
     * @brief Constructs a Throttle object.
     *
     * @param func The function to be throttled.
     * @param interval The minimum time interval between calls to the function.
     * @param leading If true, the function will be called immediately upon the
     * first call, then throttled. If false, the function will be throttled and
     * called at most once per interval.
     * @param maxWait Optional maximum wait time before invoking the function if
     * it has been called frequently. If not provided, there is no maximum wait
     * time.
     * @throws std::invalid_argument if interval is negative
     */
    explicit Throttle(
        F func, std::chrono::milliseconds interval, bool leading = false,
        std::optional<std::chrono::milliseconds> maxWait = std::nullopt);

    /**
     * @brief Invokes the throttled function if the interval has elapsed.
     */
    void operator()() noexcept;

    /**
     * @brief Cancels any pending function calls.
     */
    void cancel() noexcept;

    /**
     * @brief Resets the throttle, clearing the last call timestamp and allowing
     *        the function to be invoked immediately if required.
     */
    void reset() noexcept;

    /**
     * @brief Returns the number of times the function has been called.
     * @return The count of function invocations.
     */
    [[nodiscard]] auto callCount() const noexcept -> size_t;

private:
    F func_;  ///< The function to be throttled.
    std::chrono::milliseconds
        interval_;  ///< The time interval between allowed function calls.
    std::chrono::steady_clock::time_point
        last_call_;  ///< The timestamp of the last function call.
    mutable std::mutex
        mutex_;     ///< Mutex to protect concurrent access to internal state.
    bool leading_;  ///< Indicates if the function should be called immediately
                    ///< upon first call.
    std::atomic<bool> called_ =
        false;  ///< Flag to track if the function has been called.
    std::optional<std::chrono::milliseconds>
        maxWait_;  ///< Optional maximum wait time before invocation.
    std::atomic<size_t> call_count_{
        0};  ///< Counter to keep track of function call invocations.
};

// 实现模板类方法
template <Callable F>
Debounce<F>::Debounce(F func, std::chrono::milliseconds delay, bool leading,
                      std::optional<std::chrono::milliseconds> maxWait)
    : func_(std::move(func)),
      delay_(delay),
      leading_(leading),
      maxWait_(maxWait) {
    if (delay.count() < 0) {
        throw std::invalid_argument("Delay cannot be negative");
    }
    if (maxWait && maxWait->count() < 0) {
        throw std::invalid_argument("Max wait time cannot be negative");
    }
}

template <Callable F>
void Debounce<F>::operator()() noexcept {
    try {
        auto now = std::chrono::steady_clock::now();
        std::unique_lock lock(mutex_);

        if (leading_ && !scheduled_) {
            scheduled_ = true;
            lock.unlock();
            try {
                func_();
                ++call_count_;
            } catch (...) {
                // 记录但不传播异常
            }
            lock.lock();
        }

        last_call_ = now;
        if (!thread_.joinable()) {
            thread_ =
                std::jthread([this]([[maybe_unused]] std::stop_token stoken) {
                    this->run();
                });
        }
    } catch (...) {
        // 确保异常不传播出去
    }
}

template <Callable F>
void Debounce<F>::cancel() noexcept {
    std::unique_lock lock(mutex_);
    scheduled_ = false;
    last_call_.reset();
}

template <Callable F>
void Debounce<F>::flush() noexcept {
    try {
        std::unique_lock lock(mutex_);
        if (scheduled_) {
            scheduled_ = false;
            lock.unlock();
            try {
                func_();
                ++call_count_;
            } catch (...) {
                // 记录但不传播异常
            }
        }
    } catch (...) {
        // 确保异常不传播出去
    }
}

template <Callable F>
void Debounce<F>::reset() noexcept {
    std::unique_lock lock(mutex_);
    last_call_.reset();
    scheduled_ = false;
}

template <Callable F>
size_t Debounce<F>::callCount() const noexcept {
    return call_count_.load(std::memory_order_relaxed);
}

template <Callable F>
void Debounce<F>::run() {
    try {
        bool should_continue = true;
        while (should_continue) {
            std::this_thread::sleep_for(delay_);

            std::unique_lock lock(mutex_);
            auto now = std::chrono::steady_clock::now();

            if (last_call_ && now - last_call_.value() >= delay_) {
                if (scheduled_) {
                    scheduled_ = false;
                    lock.unlock();
                    try {
                        func_();
                        ++call_count_;
                    } catch (...) {
                        // 记录但不传播异常
                    }
                }
                should_continue = false;
            } else if (maxWait_ && last_call_ &&
                       now - last_call_.value() >= *maxWait_) {
                if (scheduled_) {
                    scheduled_ = false;
                    lock.unlock();
                    try {
                        func_();
                        ++call_count_;
                    } catch (...) {
                        // 记录但不传播异常
                    }
                }
                should_continue = false;
            }
        }
    } catch (...) {
        // 确保异常不传播出去
    }
}

template <Callable F>
Throttle<F>::Throttle(F func, std::chrono::milliseconds interval, bool leading,
                      std::optional<std::chrono::milliseconds> maxWait)
    : func_(std::move(func)),
      interval_(interval),
      last_call_(std::chrono::steady_clock::now() - interval),
      leading_(leading),
      maxWait_(maxWait) {
    if (interval.count() < 0) {
        throw std::invalid_argument("Interval cannot be negative");
    }
    if (maxWait && maxWait->count() < 0) {
        throw std::invalid_argument("Max wait time cannot be negative");
    }
}

template <Callable F>
void Throttle<F>::operator()() noexcept {
    try {
        auto now = std::chrono::steady_clock::now();
        std::unique_lock lock(mutex_);

        if (leading_ && !called_) {
            called_ = true;
            last_call_ = now;
            lock.unlock();
            try {
                func_();
                ++call_count_;
            } catch (...) {
                // 记录但不传播异常
            }
            return;
        }

        if (now - last_call_ >= interval_) {
            last_call_ = now;
            lock.unlock();
            try {
                func_();
                ++call_count_;
            } catch (...) {
                // 记录但不传播异常
            }
        } else if (maxWait_ && now - last_call_ >= *maxWait_) {
            last_call_ = now;
            lock.unlock();
            try {
                func_();
                ++call_count_;
            } catch (...) {
                // 记录但不传播异常
            }
        }
    } catch (...) {
        // 确保异常不传播出去
    }
}

template <Callable F>
void Throttle<F>::cancel() noexcept {
    std::unique_lock lock(mutex_);
    called_ = false;
}

template <Callable F>
void Throttle<F>::reset() noexcept {
    std::unique_lock lock(mutex_);
    last_call_ = std::chrono::steady_clock::now() - interval_;
    called_ = false;
}

template <Callable F>
auto Throttle<F>::callCount() const noexcept -> size_t {
    return call_count_.load(std::memory_order_relaxed);
}

}  // namespace atom::async

#endif

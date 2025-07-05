#ifndef ATOM_ASYNC_LODASH_HPP
#define ATOM_ASYNC_LODASH_HPP
/**
 * @class Debounce
 * @brief A class that implements a debouncing mechanism for function calls.
 */
#include <chrono>
#include <condition_variable>  // For std::condition_variable_any
#include <functional>          // For std::function
#include <mutex>
#include <thread>
#include <tuple>    // For std::tuple
#include <utility>  // For std::forward, std::move, std::apply
#include "atom/meta/concept.hpp"

namespace atom::async {

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
     * @throws std::invalid_argument if delay is negative.
     */
    explicit Debounce(
        F func, std::chrono::milliseconds delay, bool leading = false,
        std::optional<std::chrono::milliseconds> maxWait = std::nullopt)
        : func_(std::move(func)),
          delay_(delay),
          leading_(leading),
          maxWait_(maxWait) {
        if (delay_.count() < 0) {
            throw std::invalid_argument("Delay cannot be negative");
        }
        if (maxWait_ && maxWait_->count() < 0) {
            throw std::invalid_argument("Max wait time cannot be negative");
        }
    }

    template <typename... CallArgs>
    void operator()(CallArgs&&... args) noexcept {
        try {
            std::unique_lock lock(mutex_);
            auto now = std::chrono::steady_clock::now();

            last_call_time_ = now;

            current_task_ = [this, f = this->func_,
                             captured_args = std::make_tuple(
                                 std::forward<CallArgs>(args)...)]() mutable {
                std::apply(f, std::move(captured_args));
                this->invocation_count_.fetch_add(1, std::memory_order_relaxed);
            };

            if (!first_call_in_series_time_.has_value()) {
                first_call_in_series_time_ = now;
            }

            bool is_call_active = call_pending_.load(std::memory_order_acquire);

            if (leading_ && !is_call_active) {
                call_pending_.store(true, std::memory_order_release);

                auto task_to_run_now = current_task_;
                lock.unlock();
                try {
                    if (task_to_run_now)
                        task_to_run_now();
                } catch (...) { /* Record (e.g., log) but do not propagate
                                   exceptions */
                }
                lock.lock();
            }

            call_pending_.store(true, std::memory_order_release);

            if (timer_thread_.joinable()) {
                timer_thread_.request_stop();
                // jthread destructor/reassignment handles join. Forcing wake
                // for faster exit:
                cv_.notify_all();
            }

            timer_thread_ = std::jthread([this, task_for_timer = current_task_,
                                          timer_start_call_time =
                                              last_call_time_,
                                          timer_series_start_time =
                                              first_call_in_series_time_](
                                             std::stop_token st) {
                std::unique_lock timer_lock(mutex_);

                if (!call_pending_.load(std::memory_order_acquire)) {
                    return;
                }

                if (last_call_time_ != timer_start_call_time) {
                    return;
                }

                std::chrono::steady_clock::time_point deadline;
                if (!timer_start_call_time) {
                    call_pending_.store(false, std::memory_order_release);
                    if (first_call_in_series_time_ ==
                        timer_series_start_time) {  // reset only if this timer
                                                    // was responsible
                        first_call_in_series_time_.reset();
                    }
                    return;
                }
                deadline = timer_start_call_time.value() + delay_;

                if (maxWait_ && timer_series_start_time) {
                    std::chrono::steady_clock::time_point max_wait_deadline =
                        timer_series_start_time.value() + *maxWait_;
                    if (max_wait_deadline < deadline) {
                        deadline = max_wait_deadline;
                    }
                }

                // 修复：正确调用 wait_until，不传递 st 作为第二个参数
                bool stop_requested_during_wait =
                    cv_.wait_until(timer_lock, deadline,
                                   [&st] { return st.stop_requested(); });

                if (st.stop_requested() || stop_requested_during_wait) {
                    if (last_call_time_ != timer_start_call_time &&
                        call_pending_.load(std::memory_order_acquire)) {
                        // Superseded by a newer pending call.
                    } else if (!call_pending_.load(std::memory_order_acquire)) {
                        if (last_call_time_ == timer_start_call_time) {
                            first_call_in_series_time_.reset();
                        }
                    }
                    return;
                }

                if (call_pending_.load(std::memory_order_acquire) &&
                    last_call_time_ == timer_start_call_time) {
                    call_pending_.store(false, std::memory_order_release);
                    first_call_in_series_time_.reset();

                    timer_lock.unlock();
                    try {
                        if (task_for_timer) {
                            task_for_timer();  // This increments
                                               // invocation_count_
                        }
                    } catch (...) { /* Record (e.g., log) but do not propagate
                                       exceptions */
                    }
                } else {
                    if (!call_pending_.load(std::memory_order_acquire) &&
                        last_call_time_ == timer_start_call_time) {
                        first_call_in_series_time_.reset();
                    }
                }
            });

        } catch (...) { /* Ensure exceptions do not propagate from operator() */
        }
    }

    void cancel() noexcept {
        std::unique_lock lock(mutex_);
        call_pending_.store(false, std::memory_order_relaxed);
        first_call_in_series_time_.reset();
        current_task_ = nullptr;
        if (timer_thread_.joinable()) {
            timer_thread_.request_stop();
            cv_.notify_all();
        }
    }

    void flush() noexcept {
        try {
            std::unique_lock lock(mutex_);
            if (call_pending_.load(std::memory_order_acquire)) {
                if (timer_thread_.joinable()) {
                    timer_thread_.request_stop();
                    cv_.notify_all();
                }

                auto task_to_run = std::move(current_task_);
                call_pending_.store(false, std::memory_order_relaxed);
                first_call_in_series_time_.reset();

                if (task_to_run) {
                    lock.unlock();
                    try {
                        task_to_run();  // This increments invocation_count_
                    } catch (...) { /* Record (e.g., log) but do not propagate
                                       exceptions */
                    }
                }
            }
        } catch (...) { /* Ensure exceptions do not propagate */
        }
    }

    void reset() noexcept {
        std::unique_lock lock(mutex_);
        call_pending_.store(false, std::memory_order_relaxed);
        last_call_time_.reset();
        first_call_in_series_time_.reset();
        current_task_ = nullptr;
        if (timer_thread_.joinable()) {
            timer_thread_.request_stop();
            cv_.notify_all();
        }
    }

    [[nodiscard]] size_t callCount() const noexcept {
        return invocation_count_.load(std::memory_order_relaxed);
    }

private:
    // void run(); // Replaced by jthread lambda logic

    F func_;
    std::chrono::milliseconds delay_;
    std::optional<std::chrono::steady_clock::time_point> last_call_time_;
    std::jthread timer_thread_;
    mutable std::mutex mutex_;
    bool leading_;
    std::atomic<bool> call_pending_ = false;
    std::optional<std::chrono::milliseconds> maxWait_;
    std::atomic<size_t> invocation_count_{0};
    std::optional<std::chrono::steady_clock::time_point>
        first_call_in_series_time_;

    std::function<void()> current_task_;  // Stores the task (function + args)
    std::condition_variable_any cv_;  // For efficient waiting in timer thread
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
     * called at most once per interval (trailing edge).
     * @param trailing If true and `leading` is also true, an additional call is
     * made at the end of the throttle window if there were calls during the
     * window.
     * @throws std::invalid_argument if interval is negative.
     */
    explicit Throttle(F func, std::chrono::milliseconds interval,
                      bool leading = true, bool trailing = false);

    /**
     * @brief Attempts to invoke the throttled function.
     */
    template <typename... CallArgs>
    void operator()(CallArgs&&... args) noexcept;

    /**
     * @brief Cancels any pending trailing function call.
     */
    void cancel() noexcept;

    /**
     * @brief Resets the throttle, clearing the last call timestamp and allowing
     *        the function to be invoked immediately if `leading` is true.
     */
    void reset() noexcept;

    /**
     * @brief Returns the number of times the function has been called.
     * @return The count of function invocations.
     */
    [[nodiscard]] auto callCount() const noexcept -> size_t;

private:
    void trailingCall();

    F func_;  ///< The function to be throttled.
    std::chrono::milliseconds
        interval_;  ///< The time interval between allowed function calls.
    std::optional<std::chrono::steady_clock::time_point>
        last_call_time_;        ///< Timestamp of the last function invocation.
    mutable std::mutex mutex_;  ///< Mutex to protect concurrent access.
    bool leading_;              ///< True to invoke on the leading edge.
    bool trailing_;             ///< True to invoke on the trailing edge.
    std::atomic<size_t> invocation_count_{
        0};                         ///< Counter for actual invocations.
    std::jthread trailing_thread_;  ///< Thread for handling trailing calls.
    std::atomic<bool> trailing_call_pending_ =
        false;  ///< Is a trailing call scheduled?
    std::optional<std::chrono::steady_clock::time_point>
        last_attempt_time_;  ///< Timestamp of the last attempt to call
                             ///< operator().

    // 添加缺失的成员变量
    std::function<void()>
        current_task_payload_;  ///< Stores the current task to execute
    std::condition_variable_any
        trailing_cv_;  ///< For efficient waiting in trailing thread
};

/**
 * @class ThrottleFactory
 * @brief Factory class for creating multiple Throttle instances with the same
 * configuration.
 */
class ThrottleFactory {
public:
    /**
     * @brief Constructor.
     * @param interval Default minimum interval between calls.
     * @param leading Whether to invoke immediately on the first call.
     * @param trailing Whether to invoke on the trailing edge.
     */
    explicit ThrottleFactory(std::chrono::milliseconds interval,
                             bool leading = true, bool trailing = false)
        : interval_(interval), leading_(leading), trailing_(trailing) {}

    /**
     * @brief Creates a new Throttle instance.
     * @tparam F The type of the function.
     * @param func The function to be throttled.
     * @return A configured Throttle instance.
     */
    template <Callable F>
    [[nodiscard]] auto create(F&& func) {
        return Throttle<std::decay_t<F>>(std::forward<F>(func), interval_,
                                         leading_, trailing_);
    }

private:
    std::chrono::milliseconds interval_;
    bool leading_;
    bool trailing_;
};

/**
 * @class DebounceFactory
 * @brief Factory class for creating multiple Debounce instances with the same
 * configuration.
 */
class DebounceFactory {
public:
    /**
     * @brief Constructor.
     * @param delay The delay time.
     * @param leading Whether to invoke immediately on the first call.
     * @param maxWait Optional maximum wait time.
     */
    explicit DebounceFactory(
        std::chrono::milliseconds delay, bool leading = false,
        std::optional<std::chrono::milliseconds> maxWait = std::nullopt)
        : delay_(delay), leading_(leading), maxWait_(maxWait) {}

    /**
     * @brief Creates a new Debounce instance.
     * @tparam F The type of the function.
     * @param func The function to be debounced.
     * @return A configured Debounce instance.
     */
    template <Callable F>
    [[nodiscard]] auto create(F&& func) {
        return Debounce<std::decay_t<F>>(std::forward<F>(func), delay_,
                                         leading_, maxWait_);
    }

private:
    std::chrono::milliseconds delay_;
    bool leading_;
    std::optional<std::chrono::milliseconds> maxWait_;
};

// Implementation of Debounce methods (constructor, operator(), cancel, flush,
// reset, callCount are above) Debounce<F>::run() is removed.

// Implementation of Throttle methods
template <Callable F>
Throttle<F>::Throttle(F func, std::chrono::milliseconds interval, bool leading,
                      bool trailing)
    : func_(std::move(func)),
      interval_(interval),
      leading_(leading),
      trailing_(trailing) {
    if (interval_.count() < 0) {
        throw std::invalid_argument("Interval cannot be negative");
    }
}

template <Callable F>
template <typename... CallArgs>
void Throttle<F>::operator()(CallArgs&&... args) noexcept {
    try {
        std::unique_lock lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        last_attempt_time_ = now;

        current_task_payload_ =
            [this, f = this->func_,
             captured_args =
                 std::make_tuple(std::forward<CallArgs>(args)...)]() mutable {
                std::apply(f, std::move(captured_args));
                this->invocation_count_.fetch_add(1, std::memory_order_relaxed);
            };

        bool can_call_now = !last_call_time_.has_value() ||
                            (now - last_call_time_.value() >= interval_);

        if (leading_ && can_call_now) {
            last_call_time_ = now;
            auto task_to_run = current_task_payload_;
            lock.unlock();
            try {
                if (task_to_run)
                    task_to_run();
            } catch (...) { /* Record exceptions */
            }
            return;
        }

        if (!leading_ && can_call_now) {
            last_call_time_ = now;
            auto task_to_run = current_task_payload_;
            lock.unlock();
            try {
                if (task_to_run)
                    task_to_run();
            } catch (...) { /* Record exceptions */
            }
            return;
        }

        if (trailing_ &&
            !trailing_call_pending_.load(std::memory_order_relaxed)) {
            trailing_call_pending_.store(true, std::memory_order_relaxed);

            if (trailing_thread_.joinable()) {
                trailing_thread_.request_stop();
                trailing_cv_.notify_all();  // Wake up if waiting
            }
            trailing_thread_ = std::jthread([this, task_for_trailing =
                                                       current_task_payload_](
                                                std::stop_token st) {
                std::unique_lock trailing_lock(this->mutex_);

                if (this->interval_.count() > 0) {
                    // 修复: 正确调用 wait_for 方法
                    // 将 st 作为谓词函数的参数传递，而不是方法的第二个参数
                    if (this->trailing_cv_.wait_for(
                            trailing_lock, this->interval_,
                            [&st] { return st.stop_requested(); })) {
                        // Predicate met (stop requested) or spurious wakeup +
                        // stop_requested
                        this->trailing_call_pending_.store(
                            false, std::memory_order_relaxed);
                        return;
                    }
                    // Timeout occurred if wait_for returned false and st not
                    // requested
                    if (st.stop_requested()) {  // Double check after wait_for
                                                // if it returned due to timeout
                                                // but st became true
                        this->trailing_call_pending_.store(
                            false, std::memory_order_relaxed);
                        return;
                    }
                } else {  // Interval is zero or negative, check stop token once
                    if (st.stop_requested()) {
                        this->trailing_call_pending_.store(
                            false, std::memory_order_relaxed);
                        return;
                    }
                }

                if (this->trailing_call_pending_.load(
                        std::memory_order_acquire)) {
                    auto current_time = std::chrono::steady_clock::now();
                    if (this->last_attempt_time_ &&
                        (!this->last_call_time_.has_value() ||
                         (this->last_attempt_time_.value() >
                          this->last_call_time_.value())) &&
                        (!this->last_call_time_.has_value() ||
                         (current_time - this->last_call_time_.value() >=
                          this->interval_))) {
                        this->last_call_time_ = current_time;
                        this->trailing_call_pending_.store(
                            false, std::memory_order_relaxed);

                        trailing_lock.unlock();
                        try {
                            if (task_for_trailing)
                                task_for_trailing();  // This increments count
                        } catch (...) {               /* Record exceptions */
                        }
                        return;
                    }
                }
                this->trailing_call_pending_.store(false,
                                                   std::memory_order_relaxed);
            });
        }
    } catch (...) { /* Ensure exceptions do not propagate */
    }
}

template <Callable F>
void Throttle<F>::cancel() noexcept {
    std::unique_lock lock(mutex_);
    trailing_call_pending_.store(false, std::memory_order_relaxed);
    current_task_payload_ = nullptr;
    if (trailing_thread_.joinable()) {
        trailing_thread_.request_stop();
        trailing_cv_.notify_all();
    }
}

template <Callable F>
void Throttle<F>::reset() noexcept {
    std::unique_lock lock(mutex_);
    last_call_time_.reset();
    last_attempt_time_.reset();
    trailing_call_pending_.store(false, std::memory_order_relaxed);
    current_task_payload_ = nullptr;
    if (trailing_thread_.joinable()) {
        trailing_thread_.request_stop();
        trailing_cv_.notify_all();
    }
}

template <Callable F>
auto Throttle<F>::callCount() const noexcept -> size_t {
    return invocation_count_.load(std::memory_order_relaxed);
}
}  // namespace atom::async

#endif

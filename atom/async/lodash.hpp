#ifndef ATOM_ASYNC_LODASH_HPP
#define ATOM_ASYNC_LODASH_HPP

#include <atomic>
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

            // Store the task payload
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
                // Leading edge call
                call_pending_.store(
                    true,
                    std::memory_order_release);  // Mark as pending to prevent
                                                 // immediate subsequent leading
                                                 // calls

                auto task_to_run_now = current_task_;  // Copy the task payload
                lock.unlock();  // Release lock before running user function
                try {
                    if (task_to_run_now)
                        task_to_run_now();
                } catch (...) { /* Record (e.g., log) but do not propagate
                                   exceptions */
                }
                lock.lock();  // Re-acquire lock
                // After leading call, the debounce timer should start for
                // subsequent calls The timer thread logic below will handle
                // scheduling the trailing/delayed call
            }

            // Schedule/reschedule the delayed call
            call_pending_.store(
                true, std::memory_order_release);  // Ensure pending is true for
                                                   // the timer
            scheduled_time_ =
                now + delay_;  // Schedule based on the latest call time

            if (maxWait_ && first_call_in_series_time_) {
                auto max_wait_deadline =
                    first_call_in_series_time_.value() + *maxWait_;
                if (scheduled_time_ > max_wait_deadline) {
                    scheduled_time_ = max_wait_deadline;
                }
            }

            if (!timer_thread_.joinable() || timer_thread_.request_stop()) {
                // If thread is not running or stop was successfully requested
                // (meaning it wasn't already stopping/joining) Start a new
                // timer thread
                timer_thread_ = std::jthread([this](std::stop_token st) {
                    std::unique_lock timer_lock(mutex_);
                    while (call_pending_.load(std::memory_order_acquire) &&
                           !st.stop_requested()) {
                        auto current_scheduled_time =
                            scheduled_time_;  // Capture scheduled time under
                                              // lock
                        auto current_last_call_time =
                            last_call_time_;  // Capture last call time under
                                              // lock

                        if (!current_last_call_time) {  // Should not happen if
                                                        // call_pending is true,
                                                        // but safety check
                            call_pending_.store(false,
                                                std::memory_order_release);
                            first_call_in_series_time_.reset();
                            break;
                        }

                        // Wait until the scheduled time or stop is requested
                        bool stop_requested_during_wait = cv_.wait_until(
                            timer_lock, current_scheduled_time.value(),
                            [&st, this, current_scheduled_time]() {
                                // Predicate: stop requested OR the scheduled
                                // time has been updated to be earlier
                                return st.stop_requested() ||
                                       (scheduled_time_ &&
                                        scheduled_time_.value() <
                                            current_scheduled_time.value());
                            });

                        if (st.stop_requested() || stop_requested_during_wait) {
                            // Stop requested or scheduled time was moved
                            // earlier (handled by next loop iteration)
                            if (st.stop_requested()) {
                                // If stop was explicitly requested, clear
                                // pending flag
                                call_pending_.store(false,
                                                    std::memory_order_release);
                                first_call_in_series_time_.reset();
                            }
                            break;  // Exit thread loop
                        }

                        // Woke up because scheduled time was reached (and stop
                        // wasn't requested) Double check if the scheduled time
                        // is still the one we waited for and if a call is still
                        // pending.
                        if (call_pending_.load(std::memory_order_acquire) &&
                            scheduled_time_ &&
                            scheduled_time_.value() ==
                                current_scheduled_time.value()) {
                            // This is the correct time to fire the trailing
                            // call
                            call_pending_.store(false,
                                                std::memory_order_release);
                            first_call_in_series_time_.reset();

                            auto task_to_run =
                                current_task_;  // Copy the latest task payload
                            timer_lock.unlock();  // Release lock before running
                                                  // user function
                            try {
                                if (task_to_run) {
                                    task_to_run();  // This increments
                                                    // invocation_count_
                                }
                            } catch (...) { /* Record (e.g., log) but do not
                                               propagate exceptions */
                            }
                            return;  // Task executed, thread finishes
                        }
                        // If scheduled_time_ changed or call_pending_ became
                        // false, the loop continues or breaks
                    }
                    // Loop finished because call_pending became false or stop
                    // was requested
                    if (!call_pending_.load(std::memory_order_acquire)) {
                        first_call_in_series_time_.reset();
                    }
                });
            } else {
                // If a thread is already pending, just updating scheduled_time_
                // and notifying is enough.
                scheduled_time_ =
                    now + delay_;  // Reschedule the existing pending call
                if (maxWait_ &&
                    first_call_in_series_time_) {  // Re-apply maxWait if needed
                    auto max_wait_deadline =
                        first_call_in_series_time_.value() + *maxWait_;
                    if (scheduled_time_ > max_wait_deadline) {
                        scheduled_time_ = max_wait_deadline;
                    }
                }
                cv_.notify_one();  // Notify the waiting thread
            }

        } catch (...) { /* Ensure exceptions do not propagate from operator() */
        }
    }

    /**
     * @brief Cancels any pending delayed function call.
     */
    void cancel() noexcept {
        std::unique_lock lock(mutex_);
        call_pending_.store(false, std::memory_order_relaxed);
        last_call_time_.reset();
        first_call_in_series_time_.reset();
        scheduled_time_.reset();
        current_task_ = nullptr;
        if (timer_thread_.joinable()) {
            timer_thread_.request_stop();
            cv_.notify_all();  // Wake up the timer thread
        }
    }

    /**
     * @brief Flushes any pending delayed function call, invoking it
     * immediately.
     */
    void flush() noexcept {
        try {
            std::unique_lock lock(mutex_);
            if (call_pending_.load(std::memory_order_acquire)) {
                if (timer_thread_.joinable()) {
                    timer_thread_.request_stop();
                    cv_.notify_all();  // Wake up the timer thread
                }

                auto task_to_run =
                    std::move(current_task_);  // Get the latest task
                call_pending_.store(false, std::memory_order_relaxed);
                last_call_time_.reset();
                first_call_in_series_time_.reset();
                scheduled_time_.reset();

                if (task_to_run) {
                    lock.unlock();  // Release lock before running user function
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

    /**
     * @brief Resets the debounce state, clearing any pending calls and timers.
     */
    void reset() noexcept {
        std::unique_lock lock(mutex_);
        call_pending_.store(false, std::memory_order_relaxed);
        last_call_time_.reset();
        first_call_in_series_time_.reset();
        scheduled_time_.reset();
        current_task_ = nullptr;
        if (timer_thread_.joinable()) {
            timer_thread_.request_stop();
            cv_.notify_all();  // Wake up the timer thread
        }
    }

    /**
     * @brief Returns the number of times the debounced function has been
     * called.
     * @return The count of function invocations.
     */
    [[nodiscard]] size_t callCount() const noexcept {
        return invocation_count_.load(std::memory_order_relaxed);
    }

private:
    F func_;
    std::chrono::milliseconds delay_;
    std::optional<std::chrono::steady_clock::time_point> last_call_time_;
    std::optional<std::chrono::steady_clock::time_point> scheduled_time_;
    std::jthread timer_thread_;
    mutable std::mutex mutex_;
    bool leading_;
    std::atomic<bool> call_pending_ = false;
    std::optional<std::chrono::milliseconds> maxWait_;
    std::atomic<size_t> invocation_count_{0};
    std::optional<std::chrono::steady_clock::time_point>
        first_call_in_series_time_;

    std::function<void()> current_task_;
    std::condition_variable_any cv_;
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
    F func_;
    std::chrono::milliseconds interval_;
    std::optional<std::chrono::steady_clock::time_point> last_call_time_;
    mutable std::mutex mutex_;
    bool leading_;
    bool trailing_;
    std::atomic<size_t> invocation_count_{0};
    std::jthread trailing_thread_;
    std::atomic<bool> trailing_call_pending_ = false;
    std::optional<std::chrono::steady_clock::time_point> last_attempt_time_;

    std::function<void()> current_task_payload_;
    std::condition_variable_any trailing_cv_;
    std::optional<std::chrono::steady_clock::time_point>
        trailing_scheduled_time_;
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
        last_attempt_time_ = now;  // Record the time of this attempt

        // Store the task payload - always store the latest args
        current_task_payload_ =
            [this, f = this->func_,
             captured_args =
                 std::make_tuple(std::forward<CallArgs>(args)...)]() mutable {
                std::apply(f, std::move(captured_args));
                this->invocation_count_.fetch_add(1, std::memory_order_relaxed);
            };

        bool can_call_now = !last_call_time_.has_value() ||
                            (now - last_call_time_.value() >= interval_);

        if (can_call_now) {
            // Leading edge or simple interval call
            if (leading_ ||
                !last_call_time_.has_value()) {  // Only call immediately if
                                                 // leading or first call ever
                last_call_time_ = now;  // Update last successful call time
                auto task_to_run =
                    current_task_payload_;  // Copy the latest task
                lock.unlock();  // Release lock before running user function
                try {
                    if (task_to_run)
                        task_to_run();
                } catch (...) { /* Record exceptions */
                }
                // If leading is true, we might still need a trailing call if
                // more calls come in If leading is false, and we called now, no
                // trailing needed for this call series
                if (!leading_) {
                    // If not leading, and we just called, clear any pending
                    // trailing call
                    trailing_call_pending_.store(false,
                                                 std::memory_order_relaxed);
                    trailing_scheduled_time_.reset();
                    if (trailing_thread_.joinable()) {
                        trailing_thread_.request_stop();
                        trailing_cv_
                            .notify_all();  // Wake up the trailing thread
                    }
                }
                return;
            }
        }

        // If we couldn't call now, schedule a trailing call if enabled
        if (trailing_) {
            // Schedule the trailing call for interval_ after the *current*
            // attempt time
            auto new_scheduled_time = now + interval_;

            if (!trailing_call_pending_.load(std::memory_order_acquire)) {
                // No trailing call pending, schedule a new one
                trailing_call_pending_.store(true, std::memory_order_release);
                trailing_scheduled_time_ = new_scheduled_time;

                // Start the trailing thread if not already running
                if (!trailing_thread_.joinable() ||
                    trailing_thread_.request_stop()) {
                    trailing_thread_ = std::jthread([this](std::stop_token st) {
                        std::unique_lock trailing_lock(mutex_);
                        while (trailing_call_pending_.load(
                                   std::memory_order_acquire) &&
                               !st.stop_requested()) {
                            auto current_scheduled_time =
                                trailing_scheduled_time_;  // Capture scheduled
                                                           // time under lock

                            if (!current_scheduled_time) {  // Should not happen
                                                            // if pending is
                                                            // true
                                trailing_call_pending_.store(
                                    false, std::memory_order_release);
                                break;
                            }

                            // Wait until the scheduled time or stop is
                            // requested
                            bool stop_requested_during_wait =
                                trailing_cv_.wait_until(
                                    trailing_lock,
                                    current_scheduled_time.value(),
                                    [&st, this, current_scheduled_time]() {
                                        // Predicate: stop requested OR the
                                        // scheduled time has been updated to be
                                        // earlier
                                        return st.stop_requested() ||
                                               (trailing_scheduled_time_ &&
                                                trailing_scheduled_time_
                                                        .value() <
                                                    current_scheduled_time
                                                        .value());
                                    });

                            if (st.stop_requested() ||
                                stop_requested_during_wait) {
                                // Stop requested or scheduled time was moved
                                // earlier (handled by next loop iteration)
                                if (st.stop_requested()) {
                                    // If stop was explicitly requested, clear
                                    // pending flag
                                    trailing_call_pending_.store(
                                        false, std::memory_order_release);
                                }
                                break;  // Exit thread loop
                            }

                            // Woke up because scheduled time was reached (and
                            // stop wasn't requested) Double check if the
                            // scheduled time is still the one we waited for and
                            // if a call is still pending.
                            if (trailing_call_pending_.load(
                                    std::memory_order_acquire) &&
                                trailing_scheduled_time_ &&
                                trailing_scheduled_time_.value() ==
                                    current_scheduled_time.value()) {
                                // This is the correct time to fire the trailing
                                // call
                                auto current_time =
                                    std::chrono::steady_clock::now();
                                last_call_time_ =
                                    current_time;  // Update last successful
                                                   // call time
                                trailing_call_pending_.store(
                                    false, std::memory_order_release);
                                trailing_scheduled_time_
                                    .reset();  // Clear scheduled time

                                auto task_to_run =
                                    current_task_payload_;  // Copy the latest
                                                            // task payload
                                trailing_lock
                                    .unlock();  // Release lock before running
                                                // user function
                                try {
                                    if (task_to_run) {
                                        task_to_run();  // This increments
                                                        // invocation_count_
                                    }
                                } catch (...) { /* Record (e.g., log) but do not
                                                   propagate exceptions */
                                }
                                return;  // Task executed, thread finishes
                            }
                            // If scheduled_time_ changed or
                            // trailing_call_pending_ became false, the loop
                            // continues or breaks
                        }
                        // Loop finished because trailing_call_pending became
                        // false or stop was requested
                    });
                } else {
                    // Trailing is enabled and a call is already pending.
                    // Just update the scheduled time based on the latest
                    // attempt. The waiting thread will pick up the new
                    // scheduled time. Only update if the new scheduled time is
                    // *later* than the current one, unless we want to allow
                    // shortening the wait? Standard is usually extend.
                    if (!trailing_scheduled_time_ ||
                        new_scheduled_time > trailing_scheduled_time_.value()) {
                        trailing_scheduled_time_ = new_scheduled_time;
                        trailing_cv_
                            .notify_one();  // Notify the waiting thread
                                            // about the updated schedule
                    }
                }
            }
        }

    } catch (...) { /* Ensure exceptions do not propagate */
    }
}

template <Callable F>
void Throttle<F>::cancel() noexcept {
    std::unique_lock lock(mutex_);
    trailing_call_pending_.store(false, std::memory_order_relaxed);
    trailing_scheduled_time_.reset();
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
    trailing_scheduled_time_.reset();
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
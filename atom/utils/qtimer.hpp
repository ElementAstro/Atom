#ifndef ATOM_UTILS_QTIMER_HPP
#define ATOM_UTILS_QTIMER_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace atom::utils {

// Concept for duration types
template <typename T>
concept ChronoDuration = std::is_convertible_v<
    T, std::chrono::duration<typename T::rep, typename T::period>>;

/**
 * @brief Class to measure elapsed time using std::chrono.
 *
 * This class provides functionality to measure elapsed time in various units
 * (nanoseconds, microseconds, milliseconds, seconds, minutes, hours). It uses
 * std::chrono for precise time measurements.
 */
class ElapsedTimer {
public:
    using Clock = std::chrono::steady_clock;
    using Nanoseconds = std::chrono::nanoseconds;
    using Microseconds = std::chrono::microseconds;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;
    using Minutes = std::chrono::minutes;
    using Hours = std::chrono::hours;

    /**
     * @brief Default constructor.
     *
     * Initializes the timer. The timer is initially not started.
     */
    ElapsedTimer() = default;

    /**
     * @brief Constructor that starts the timer immediately.
     *
     * @param start_now If true, the timer starts immediately.
     */
    explicit ElapsedTimer(bool start_now);

    /**
     * @brief Start or restart the timer.
     *
     * Sets the start time of the timer to the current time.
     */
    void start();

    /**
     * @brief Invalidate the timer.
     *
     * Resets the start time of the timer to an invalid state.
     */
    void invalidate();

    /**
     * @brief Check if the timer has been started and is valid.
     *
     * @return true if the timer is valid and started, false otherwise.
     */
    [[nodiscard]] auto isValid() const noexcept -> bool;

    /**
     * @brief Get elapsed time in a specified duration type.
     *
     * @tparam DurationType The duration type to return.
     * @return Elapsed time in the specified duration type since the timer was
     * started. Returns 0 if the timer is not valid.
     * @throws std::logic_error if the timer is not valid and throw_if_invalid
     * is true.
     */
    template <ChronoDuration DurationType, bool throw_if_invalid = false>
    [[nodiscard]] auto elapsed() const -> typename DurationType::rep {
        if (!isValid()) {
            if constexpr (throw_if_invalid) {
                throw std::logic_error("Timer is not valid");
            }
            return 0;
        }

        return std::chrono::duration_cast<DurationType>(Clock::now() -
                                                        start_time_.value())
            .count();
    }

    /**
     * @brief Get elapsed time in nanoseconds.
     *
     * @return Elapsed time in nanoseconds since the timer was started.
     *         Returns 0 if the timer is not valid.
     */
    [[nodiscard]] auto elapsedNs() const -> int64_t;

    /**
     * @brief Get elapsed time in microseconds.
     *
     * @return Elapsed time in microseconds since the timer was started.
     *         Returns 0 if the timer is not valid.
     */
    [[nodiscard]] auto elapsedUs() const -> int64_t;

    /**
     * @brief Get elapsed time in milliseconds.
     *
     * @return Elapsed time in milliseconds since the timer was started.
     *         Returns 0 if the timer is not valid.
     */
    [[nodiscard]] auto elapsedMs() const -> int64_t;

    /**
     * @brief Get elapsed time in seconds.
     *
     * @return Elapsed time in seconds since the timer was started.
     *         Returns 0 if the timer is not valid.
     */
    [[nodiscard]] auto elapsedSec() const -> int64_t;

    /**
     * @brief Get elapsed time in minutes.
     *
     * @return Elapsed time in minutes since the timer was started.
     *         Returns 0 if the timer is not valid.
     */
    [[nodiscard]] auto elapsedMin() const -> int64_t;

    /**
     * @brief Get elapsed time in hours.
     *
     * @return Elapsed time in hours since the timer was started.
     *         Returns 0 if the timer is not valid.
     */
    [[nodiscard]] auto elapsedHrs() const -> int64_t;

    /**
     * @brief Get elapsed time in milliseconds (same as elapsedMs).
     *
     * @return Elapsed time in milliseconds since the timer was started.
     *         Returns 0 if the timer is not valid.
     */
    [[nodiscard]] auto elapsed() const -> int64_t;

    /**
     * @brief Check if a specified duration (in milliseconds) has passed.
     *
     * @param ms Duration in milliseconds to check against elapsed time. Must be
     * non-negative.
     * @return true if the specified duration has passed, false otherwise.
     * @throws std::invalid_argument if ms is negative.
     */
    [[nodiscard]] auto hasExpired(int64_t ms) const -> bool;

    /**
     * @brief Get the remaining time until the specified duration (in
     * milliseconds) has passed.
     *
     * @param ms Duration in milliseconds to check against elapsed time. Must be
     * non-negative.
     * @return Remaining time in milliseconds until the specified duration
     * passes. Returns 0 if the duration has already passed or the timer is
     * invalid.
     * @throws std::invalid_argument if ms is negative.
     */
    [[nodiscard]] auto remainingTimeMs(int64_t ms) const -> int64_t;

    /**
     * @brief Get the current absolute time in milliseconds since epoch.
     *
     * @return Current time in milliseconds since epoch.
     */
    static auto currentTimeMs() noexcept -> int64_t;

    // Comparison operators
    [[nodiscard]] auto operator<=>(const ElapsedTimer& other) const noexcept -> std::strong_ordering;
    [[nodiscard]] auto operator==(const ElapsedTimer& other) const noexcept -> bool;

private:
    std::optional<Clock::time_point> start_time_;  ///< Start time of the timer.
};

/**
 * @brief Timer exception class for specific timer-related errors.
 *
 * This class provides more specific error information for timer-related exceptions.
 */
class TimerException : public std::runtime_error {
public:
    enum class ErrorCode {
        INVALID_INTERVAL,
        TIMER_ALREADY_ACTIVE,
        TIMER_NOT_ACTIVE,
        CALLBACK_EXECUTION_ERROR,
        THREAD_CREATION_ERROR
    };

    TimerException(ErrorCode code, const std::string& message)
        : std::runtime_error(message), error_code_(code) {}

    [[nodiscard]] auto errorCode() const noexcept -> ErrorCode { return error_code_; }

private:
    ErrorCode error_code_;
};

/**
 * @brief Modern C++ timer class inspired by Qt's QTimer but with modern C++ features.
 * 
 * This class provides timer functionality similar to Qt's QTimer but uses modern C++
 * features like std::function for callbacks, std::thread for timing, and strong
 * exception safety guarantees.
 */
class Timer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Callback = std::function<void()>;
    
    /**
     * @brief Timer precision modes
     */
    enum class PrecisionMode {
        PRECISE,  ///< More CPU intensive but more precise timing
        COARSE    ///< Less CPU intensive but less precise timing
    };
    
    /**
     * @brief Default constructor
     */
    Timer() = default;
    
    /**
     * @brief Constructor with callback
     * @param callback Function to call when timer expires
     */
    explicit Timer(Callback callback);
    
    /**
     * @brief Destructor
     * 
     * Ensures the timer thread is stopped properly
     */
    ~Timer();
    
    // Disable copy to avoid thread safety issues
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    
    // Allow move operations
    Timer(Timer&& other) noexcept;
    Timer& operator=(Timer&& other) noexcept;
    
    /**
     * @brief Sets the callback function
     * @param callback Function to call when timer expires
     */
    void setCallback(Callback callback);
    
    /**
     * @brief Sets the interval between timeouts
     * @param milliseconds Interval in milliseconds (must be positive)
     * @throws TimerException if milliseconds is not positive
     */
    void setInterval(int64_t milliseconds);
    
    /**
     * @brief Gets the current interval
     * @return Current interval in milliseconds
     */
    [[nodiscard]] auto interval() const noexcept -> int64_t;
    
    /**
     * @brief Sets the precision mode
     * @param mode Precision mode (PRECISE or COARSE)
     */
    void setPrecisionMode(PrecisionMode mode) noexcept;
    
    /**
     * @brief Gets the current precision mode
     * @return Current precision mode
     */
    [[nodiscard]] auto precisionMode() const noexcept -> PrecisionMode;
    
    /**
     * @brief Sets whether the timer is a single-shot timer
     * @param singleShot If true, timer fires only once
     */
    void setSingleShot(bool singleShot) noexcept;
    
    /**
     * @brief Checks if timer is set to single-shot mode
     * @return True if timer is in single-shot mode
     */
    [[nodiscard]] auto isSingleShot() const noexcept -> bool;
    
    /**
     * @brief Checks if timer is currently active
     * @return True if timer is active
     */
    [[nodiscard]] auto isActive() const noexcept -> bool;
    
    /**
     * @brief Starts or restarts the timer
     * @throws TimerException if callback is not set or on thread creation failure
     */
    void start();
    
    /**
     * @brief Starts or restarts the timer with a specified interval
     * @param milliseconds Interval in milliseconds
     * @throws TimerException if milliseconds is not positive or on thread creation failure
     */
    void start(int64_t milliseconds);
    
    /**
     * @brief Stops the timer
     */
    void stop();
    
    /**
     * @brief Creates a single-shot timer that calls the provided callback after the specified interval
     * @param milliseconds Interval in milliseconds
     * @param callback Function to call when timer expires
     * @param mode Precision mode
     * @return Shared pointer to the created timer
     * @throws TimerException if milliseconds is not positive
     */
    static auto singleShot(int64_t milliseconds, Callback callback, 
                          PrecisionMode mode = PrecisionMode::PRECISE) 
        -> std::shared_ptr<Timer>;

    /**
     * @brief Gets the time remaining before the next timeout
     * @return Remaining time in milliseconds, 0 if timer is not active
     */
    [[nodiscard]] auto remainingTime() const -> int64_t;

private:
    void timerLoop();
    
    Callback callback_;
    std::atomic<int64_t> interval_{0};
    std::atomic<bool> is_active_{false};
    std::atomic<bool> is_single_shot_{false};
    std::atomic<PrecisionMode> precision_mode_{PrecisionMode::PRECISE};
    
    mutable std::mutex timer_mutex_;
    std::thread timer_thread_;
    std::atomic<bool> should_stop_{false};
    std::optional<TimePoint> next_timeout_;
};

}  // namespace atom::utils

#endif  // ATOM_UTILS_QTIMER_HPP

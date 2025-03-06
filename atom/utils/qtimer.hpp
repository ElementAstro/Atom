#ifndef ATOM_UTILS_QTIMER_HPP
#define ATOM_UTILS_QTIMER_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
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

}  // namespace atom::utils

#endif  // ATOM_UTILS_QTIMER_HPP

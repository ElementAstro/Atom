#ifndef ATOM_UTILS_STOPWATCHER_HPP
#define ATOM_UTILS_STOPWATCHER_HPP

#include <concepts>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "atom/error/exception.hpp"

namespace atom::utils {
class StopWatcherException : public atom::error::Exception {
public:
    using Exception::Exception;
};

#define THROW_STOPWATCHER_EXCEPTION(...)     \
    throw atom::utils::StopWatcherException( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__);

/**
 * @brief States that a StopWatcher instance can be in
 */
enum class StopWatcherState {
    Idle,     ///< Initial state, before first start
    Running,  ///< Timer is currently running
    Paused,   ///< Timer is paused, can be resumed
    Stopped   ///< Timer is stopped, must be reset before starting again
};

/**
 * @brief Statistics from lap times
 */
struct LapStatistics {
    double min;          ///< Minimum lap time
    double max;          ///< Maximum lap time
    double average;      ///< Average lap time
    double standardDev;  ///< Standard deviation of lap times
    size_t count;        ///< Number of laps

    // Allow constexpr construction for compile-time statistics
    constexpr LapStatistics(double min = 0.0, double max = 0.0,
                            double avg = 0.0, double std = 0.0, size_t cnt = 0)
        : min(min), max(max), average(avg), standardDev(std), count(cnt) {}
};

// Concept for valid callback functions
template <typename T>
concept ValidCallback =
    std::invocable<T> && std::same_as<std::invoke_result_t<T>, void>;

// Error codes for StopWatcher operations
enum class StopWatcherError {
    AlreadyRunning,
    NotRunning,
    NotPaused,
    InvalidInterval,
    CallbackFailed
};

/**
 * @brief A high-precision stopwatch class for timing operations
 * @details This class provides functionality to measure elapsed time with
 * millisecond precision. It supports operations like start, stop, pause, resume
 * and lap timing. The class is thread-safe and uses RAII principles.
 *
 * Example usage:
 * @code
 * StopWatcher sw;
 * sw.start();
 * // ... do some work ...
 * sw.lap();    // Record intermediate time
 * // ... do more work ...
 * sw.stop();
 * std::cout << "Total time: " << sw.elapsedFormatted() << std::endl;
 * @endcode
 */
class StopWatcher {
public:
    /**
     * @brief Constructs a new StopWatcher instance
     * @param name Optional name for this stopwatch instance for identification
     * @throws std::bad_alloc if memory allocation fails
     */
    explicit StopWatcher(std::string_view name = "");

    /**
     * @brief Destructor
     */
    ~StopWatcher();

    // Delete copy operations to prevent resource sharing
    StopWatcher(const StopWatcher&) = delete;
    auto operator=(const StopWatcher&) -> StopWatcher& = delete;

    /**
     * @brief Move constructor
     * @note This operation is noexcept and cannot fail
     */
    StopWatcher(StopWatcher&&) noexcept;

    /**
     * @brief Move assignment operator
     * @return StopWatcher& Reference to the moved-to object
     * @note This operation is noexcept and cannot fail
     */
    auto operator=(StopWatcher&&) noexcept -> StopWatcher&;

    /**
     * @brief Starts the stopwatch
     * @return std::expected<void, StopWatcherError> Success or error code
     * @note Thread-safe
     */
    [[nodiscard]] auto start() -> std::expected<void, StopWatcherError>;

    /**
     * @brief Stops the stopwatch
     * @note Thread-safe
     * @return std::expected<void, StopWatcherError> Success or error code
     */
    [[nodiscard]] auto stop() -> std::expected<void, StopWatcherError>;

    /**
     * @brief Pauses the stopwatch without resetting
     * @note Thread-safe
     * @return std::expected<void, StopWatcherError> Success or error code
     */
    [[nodiscard]] auto pause() -> std::expected<void, StopWatcherError>;

    /**
     * @brief Resumes the stopwatch from paused state
     * @note Thread-safe
     * @return std::expected<void, StopWatcherError> Success or error code
     */
    [[nodiscard]] auto resume() -> std::expected<void, StopWatcherError>;

    /**
     * @brief Resets the stopwatch to initial state
     * @note Clears all recorded lap times and callbacks
     * @note Thread-safe
     */
    void reset();

    /**
     * @brief Gets the elapsed time in milliseconds
     * @return double The elapsed time with millisecond precision
     * @note Thread-safe, constexpr-compatible
     */
    [[nodiscard]] auto elapsedMilliseconds() const -> double;

    /**
     * @brief Gets the elapsed time in seconds
     * @return double The elapsed time with second precision
     * @note Thread-safe, constexpr-compatible
     */
    [[nodiscard]] auto elapsedSeconds() const -> double;

    /**
     * @brief Gets the elapsed time as formatted string (HH:MM:SS.mmm)
     * @return std::string Formatted time string
     * @note Thread-safe
     */
    [[nodiscard]] auto elapsedFormatted() const -> std::string;

    /**
     * @brief Gets the current state of the stopwatch
     * @return StopWatcherState Current state
     * @note Thread-safe, constexpr-compatible
     */
    [[nodiscard]] auto getState() const -> StopWatcherState;

    /**
     * @brief Gets all recorded lap times
     * @return std::span<const double> View of lap times in milliseconds
     * @note Thread-safe
     */
    [[nodiscard]] auto getLapTimes() const -> std::span<const double>;

    /**
     * @brief Gets the average of all recorded lap times
     * @return double Average lap time in milliseconds, 0 if no laps recorded
     * @note Thread-safe, constexpr-compatible
     */
    [[nodiscard]] auto getAverageLapTime() const -> double;

    /**
     * @brief Gets comprehensive statistics about lap times
     * @return LapStatistics Structure with statistical information
     * @note Thread-safe
     */
    [[nodiscard]] auto getLapStatistics() const -> LapStatistics;

    /**
     * @brief Gets the total number of laps recorded
     * @return size_t Number of laps
     * @note Thread-safe, constexpr-compatible
     */
    [[nodiscard]] auto getLapCount() const -> size_t;

    /**
     * @brief Registers a callback to be called after specified time
     * @param callback Function to be called
     * @param milliseconds Time in milliseconds after which callback should
     * trigger
     * @return std::expected<void, StopWatcherError> Success or error code
     * @note Thread-safe
     */
    template <ValidCallback CallbackType>
    auto registerCallback(CallbackType&& callback, int milliseconds)
        -> std::expected<void, StopWatcherError> {
        return registerCallbackImpl(std::forward<CallbackType>(callback),
                                    milliseconds);
    }

    /**
     * @brief Records current time as a lap time
     * @return std::expected<double, StopWatcherError> The recorded lap time in
     * milliseconds or error
     * @note Thread-safe
     */
    [[nodiscard]] auto lap() -> std::expected<double, StopWatcherError>;

    /**
     * @brief Enables automatic lap recording at specified intervals
     * @param intervalMs Interval in milliseconds between automatic laps
     * @return std::expected<void, StopWatcherError> Success or error code
     * @note Creates a background thread that records laps at regular intervals
     */
    [[nodiscard]] auto enableAutoLap(int intervalMs)
        -> std::expected<void, StopWatcherError>;

    /**
     * @brief Disables automatic lap recording
     */
    void disableAutoLap();

    /**
     * @brief Checks if the stopwatch is running
     * @return bool True if running, false otherwise
     * @note Thread-safe, constexpr-compatible
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Gets the name of this stopwatch instance
     * @return std::string_view The name of the stopwatch
     */
    [[nodiscard]] auto getName() const -> std::string_view;

    /**
     * @brief Creates a nested child stopwatch
     * @param name Name for the child stopwatch
     * @return std::unique_ptr<StopWatcher> A new stopwatch instance that is
     * logically a child
     * @note The parent-child relationship is used for hierarchical timing and
     * reporting
     */
    [[nodiscard]] auto createChildStopWatch(std::string_view name)
        -> std::unique_ptr<StopWatcher>;

    /**
     * @brief Serializes the stopwatch data to JSON format
     * @return std::string JSON representation of timing data
     */
    [[nodiscard]] auto toJson() const -> std::string;

    /**
     * @brief Creates a stopwatch from serialized JSON data
     * @param json JSON string with timing data
     * @return std::unique_ptr<StopWatcher> New stopwatch initialized with the
     * data
     */
    [[nodiscard]] static auto fromJson(std::string_view json)
        -> std::unique_ptr<StopWatcher>;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;  ///< Implementation pointer (PIMPL idiom)

    /**
     * @brief Protected method to add a lap time during deserialization
     * @param lapTime The lap time to add in milliseconds
     * @note This method should only be used during deserialization
     */
    void addLapTimeForDeserialization(double lapTime);

    auto registerCallbackImpl(std::function<void()> callback, int milliseconds)
        -> std::expected<void, StopWatcherError>;
};

/**
 * @brief RAII helper that automatically starts timing on construction and stops
 * on destruction
 */
class ScopedStopWatch {
public:
    /**
     * @brief Creates and starts a scoped stopwatch
     * @param name Optional name for this timing operation
     */
    explicit ScopedStopWatch(std::string_view name = "");

    /**
     * @brief Stops timing and logs the elapsed time
     */
    ~ScopedStopWatch();

    /**
     * @brief Gets the underlying stopwatch
     * @return const StopWatcher& Reference to the stopwatch
     */
    [[nodiscard]] auto getStopWatcher() const -> const StopWatcher&;

private:
    StopWatcher stopwatch_;
};

}  // namespace atom::utils
#endif
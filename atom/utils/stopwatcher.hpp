#ifndef ATOM_UTILS_STOPWATCHER_HPP
#define ATOM_UTILS_STOPWATCHER_HPP

#include <functional>
#include <memory>
#include <string>

namespace atom::utils {

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
     * @throws std::bad_alloc if memory allocation fails
     */
    StopWatcher();

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
     * @throws std::runtime_error if the stopwatch is already running
     * @note Thread-safe
     */
    void start();

    /**
     * @brief Stops the stopwatch
     * @note Thread-safe
     */
    void stop();

    /**
     * @brief Pauses the stopwatch without resetting
     * @throws std::runtime_error if the stopwatch is not running
     * @note Thread-safe
     */
    void pause();

    /**
     * @brief Resumes the stopwatch from paused state
     * @throws std::runtime_error if the stopwatch is not paused
     * @note Thread-safe
     */
    void resume();

    /**
     * @brief Resets the stopwatch to initial state
     * @note Clears all recorded lap times and callbacks
     * @note Thread-safe
     */
    void reset();

    /**
     * @brief Gets the elapsed time in milliseconds
     * @return double The elapsed time with millisecond precision
     * @note Thread-safe
     */
    [[nodiscard]] auto elapsedMilliseconds() const -> double;

    /**
     * @brief Gets the elapsed time in seconds
     * @return double The elapsed time with second precision
     * @note Thread-safe
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
     * @note Thread-safe
     */
    [[nodiscard]] auto getState() const -> StopWatcherState;

    /**
     * @brief Gets all recorded lap times
     * @return std::vector<double> Vector of lap times in milliseconds
     * @note Thread-safe
     */
    [[nodiscard]] auto getLapTimes() const -> std::vector<double>;

    /**
     * @brief Gets the average of all recorded lap times
     * @return double Average lap time in milliseconds, 0 if no laps recorded
     * @note Thread-safe
     */
    [[nodiscard]] auto getAverageLapTime() const -> double;

    /**
     * @brief Registers a callback to be called after specified time
     * @param callback Function to be called
     * @param milliseconds Time in milliseconds after which callback should
     * trigger
     * @throws std::invalid_argument if milliseconds is negative
     * @note Thread-safe
     */
    void registerCallback(std::function<void()> callback, int milliseconds);

    /**
     * @brief Records current time as a lap time
     * @throws std::runtime_error if stopwatch is not running
     * @note Thread-safe
     */
    void lap();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;  ///< Implementation pointer (iMPL_ idiom)
};

}  // namespace atom::utils
#endif
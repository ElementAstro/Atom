/**
 * @file progress_reporter.hpp
 * @brief Progress tracking and reporting for OCR operations
 */

#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

namespace atom::image::ocr {

/**
 * @class ProgressReporter
 * @brief Tracks and reports progress of OCR operations
 *
 * Provides thread-safe progress tracking and reporting for long-running
 * OCR operations with timing information.
 */
class ProgressReporter {
public:
    /**
     * @brief Construct a new ProgressReporter
     * @param taskName Name of the task
     * @param total Total work units expected
     */
    ProgressReporter(std::string taskName, size_t total);

    ~ProgressReporter() = default;

    /**
     * @brief Update progress counter
     * @param increment Number of work units completed (default 1)
     */
    void update(size_t increment = 1);

    /**
     * @brief Set new total work units
     * @param total New total work units
     */
    void setTotal(size_t total);

    /**
     * @brief Print current progress to stdout
     */
    void reportProgress() const;

    /**
     * @brief Get current progress percentage
     * @return Progress percentage (0-100)
     */
    float getPercentage() const;

    /**
     * @brief Get elapsed time in seconds
     * @return Elapsed seconds since construction
     */
    int64_t getElapsedSeconds() const;

    /**
     * @brief Get estimated time remaining in seconds
     * @return Estimated remaining seconds, or -1 if unknown
     */
    int64_t getETASeconds() const;

    /**
     * @brief Reset progress counter
     */
    void reset();

    /**
     * @brief Check if task is complete
     * @return True if current progress >= total
     */
    bool isComplete() const;

private:
    std::string m_taskName;            ///< Name of the task being tracked
    size_t m_total;                    ///< Total work units
    std::atomic<size_t> m_current{0};  ///< Current progress
    std::chrono::time_point<std::chrono::steady_clock>
        m_startTime;             ///< Start time
    mutable std::mutex m_mutex;  ///< Mutex for thread safety
};

}  // namespace atom::image::ocr

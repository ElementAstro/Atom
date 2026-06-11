#ifndef CRON_JOB_HPP
#define CRON_JOB_HPP

#include <chrono>
#include <string>
#include <vector>
#include <deque>
#include <atomic>
#include <mutex>
#include <optional>
#include "atom/type/json_fwd.hpp"

/**
 * @brief Execution result entry for history tracking
 */
struct ExecutionEntry {
    std::chrono::system_clock::time_point timestamp;
    bool success;
    std::string error_message;  // Optional error details

    ExecutionEntry(std::chrono::system_clock::time_point ts, bool s, std::string err = "")
        : timestamp(ts), success(s), error_message(std::move(err)) {}
};

/**
 * @brief Job status enumeration for better type safety
 */
enum class JobStatus : uint8_t {
    DISABLED = 0,
    ENABLED = 1,
    PAUSED = 2,
    FAILED = 3
};

/**
 * @brief Job priority levels
 */
enum class JobPriority : uint8_t {
    LOWEST = 1,
    LOW = 3,
    NORMAL = 5,
    HIGH = 7,
    HIGHEST = 9,
    CRITICAL = 10
};

/**
 * @brief Represents a Cron job with optimized memory layout and performance.
 *
 * Optimizations:
 * - Compact memory layout with proper alignment
 * - Reduced object size through bit fields and enums
 * - Efficient execution history with circular buffer
 * - Move semantics support
 * - Thread-safe operations where needed
 */
class alignas(64) CronJob {
public:
    // Core job data - most frequently accessed
    std::string time_;
    std::string command_;

private:
    // Compact status and configuration data
    JobStatus status_ : 3;
    JobPriority priority_ : 4;
    bool one_time_ : 1;

    // Counters and retry logic
    std::atomic<uint32_t> run_count_{0};
    uint8_t max_retries_{0};
    uint8_t current_retries_{0};

    // Timestamps
    std::chrono::system_clock::time_point created_at_;
    std::atomic<std::chrono::system_clock::time_point> last_run_;

    // String data - less frequently accessed
    std::string category_;
    std::string description_;

    // Execution history with efficient storage
    mutable std::mutex history_mutex_;
    std::deque<ExecutionEntry> execution_history_;
    static constexpr size_t MAX_HISTORY_SIZE = 100;

public:

    /**
     * @brief Constructs a new CronJob object with optimized initialization.
     * @param time Scheduled time for the Cron job
     * @param command Command to be executed by the Cron job
     * @param enabled Status of the Cron job
     * @param category Category of the Cron job for organization
     * @param description Description of what the job does
     */
    CronJob(const std::string& time = "", const std::string& command = "",
            bool enabled = true, const std::string& category = "default",
            const std::string& description = "")
        : time_(time),
          command_(command),
          status_(enabled ? JobStatus::ENABLED : JobStatus::DISABLED),
          priority_(JobPriority::NORMAL),
          one_time_(false),
          run_count_(0),
          max_retries_(0),
          current_retries_(0),
          created_at_(std::chrono::system_clock::now()),
          last_run_(std::chrono::system_clock::time_point()),
          category_(category),
          description_(description) {
        // deque doesn't have reserve, but we can pre-allocate if needed
    }

    /**
     * @brief Move constructor for efficient object transfer
     */
    CronJob(CronJob&& other) noexcept
        : time_(std::move(other.time_)),
          command_(std::move(other.command_)),
          status_(other.status_),
          priority_(other.priority_),
          one_time_(other.one_time_),
          run_count_(other.run_count_.load()),
          max_retries_(other.max_retries_),
          current_retries_(other.current_retries_),
          created_at_(other.created_at_),
          last_run_(other.last_run_.load()),
          category_(std::move(other.category_)),
          description_(std::move(other.description_)),
          execution_history_(std::move(other.execution_history_)) {
    }

    /**
     * @brief Move assignment operator
     */
    CronJob& operator=(CronJob&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(history_mutex_);
            std::lock_guard<std::mutex> other_lock(other.history_mutex_);

            time_ = std::move(other.time_);
            command_ = std::move(other.command_);
            status_ = other.status_;
            priority_ = other.priority_;
            one_time_ = other.one_time_;
            run_count_ = other.run_count_.load();
            max_retries_ = other.max_retries_;
            current_retries_ = other.current_retries_;
            created_at_ = other.created_at_;
            last_run_ = other.last_run_.load();
            category_ = std::move(other.category_);
            description_ = std::move(other.description_);
            execution_history_ = std::move(other.execution_history_);
        }
        return *this;
    }

    /**
     * @brief Copy constructor with value-snapshot semantics.
     *
     * CronJob owns a mutex and atomic counters, so the compiler-generated
     * copy is ill-formed. We provide an explicit copy that snapshots the
     * source's values under its history lock; the new object gets its own
     * fresh mutex. Copyability is required by value-based APIs
     * (createCronJob(CronJob), Python bindings, test fixtures).
     */
    CronJob(const CronJob& other)
        : time_(other.time_),
          command_(other.command_),
          status_(other.status_),
          priority_(other.priority_),
          one_time_(other.one_time_),
          run_count_(other.run_count_.load()),
          max_retries_(other.max_retries_),
          current_retries_(other.current_retries_),
          created_at_(other.created_at_),
          last_run_(other.last_run_.load()),
          category_(other.category_),
          description_(other.description_) {
        std::lock_guard<std::mutex> lock(other.history_mutex_);
        execution_history_ = other.execution_history_;
    }

    /**
     * @brief Copy assignment with value-snapshot semantics.
     */
    CronJob& operator=(const CronJob& other) {
        if (this != &other) {
            std::scoped_lock lock(history_mutex_, other.history_mutex_);
            time_ = other.time_;
            command_ = other.command_;
            status_ = other.status_;
            priority_ = other.priority_;
            one_time_ = other.one_time_;
            run_count_ = other.run_count_.load();
            max_retries_ = other.max_retries_;
            current_retries_ = other.current_retries_;
            created_at_ = other.created_at_;
            last_run_ = other.last_run_.load();
            category_ = other.category_;
            description_ = other.description_;
            execution_history_ = other.execution_history_;
        }
        return *this;
    }

    // Optimized getters with proper const-correctness
    [[nodiscard]] const std::string& getTime() const noexcept { return time_; }
    [[nodiscard]] const std::string& getCommand() const noexcept { return command_; }
    [[nodiscard]] bool isEnabled() const noexcept { return status_ == JobStatus::ENABLED; }
    [[nodiscard]] bool isPaused() const noexcept { return status_ == JobStatus::PAUSED; }
    [[nodiscard]] bool isFailed() const noexcept { return status_ == JobStatus::FAILED; }
    [[nodiscard]] JobStatus getStatus() const noexcept { return status_; }
    [[nodiscard]] JobPriority getPriority() const noexcept { return priority_; }
    [[nodiscard]] bool isOneTime() const noexcept { return one_time_; }
    [[nodiscard]] uint32_t getRunCount() const noexcept { return run_count_.load(); }
    [[nodiscard]] uint8_t getMaxRetries() const noexcept { return max_retries_; }
    [[nodiscard]] uint8_t getCurrentRetries() const noexcept { return current_retries_; }
    [[nodiscard]] auto getCreatedAt() const noexcept -> std::chrono::system_clock::time_point { return created_at_; }
    [[nodiscard]] auto getLastRun() const noexcept -> std::chrono::system_clock::time_point { return last_run_.load(); }
    [[nodiscard]] const std::string& getCategory() const noexcept { return category_; }
    [[nodiscard]] const std::string& getDescription() const noexcept { return description_; }

    // Optimized setters
    void setStatus(JobStatus status) noexcept { status_ = status; }
    void setPriority(JobPriority priority) noexcept { priority_ = priority; }
    void setOneTime(bool one_time) noexcept { one_time_ = one_time; }
    void setMaxRetries(uint8_t max_retries) noexcept { max_retries_ = max_retries; }
    void setCategory(std::string category) { category_ = std::move(category); }
    void setDescription(std::string description) { description_ = std::move(description); }

    // Enable/disable operations
    void enable() noexcept { status_ = JobStatus::ENABLED; }
    void disable() noexcept { status_ = JobStatus::DISABLED; }
    void pause() noexcept { status_ = JobStatus::PAUSED; }
    void markFailed() noexcept { status_ = JobStatus::FAILED; }

    // Retry management
    bool canRetry() const noexcept { return current_retries_ < max_retries_; }
    void incrementRetries() noexcept { ++current_retries_; }
    void resetRetries() noexcept { current_retries_ = 0; }

    /**
     * @brief Gets a unique identifier for this job.
     * @return A string that uniquely identifies this job.
     */
    [[nodiscard]] auto getId() const -> std::string;

    /**
     * @brief Records an execution result in the job's history with thread safety.
     * @param success Whether the execution was successful.
     * @param error_message Optional error message for failed executions.
     */
    void recordExecution(bool success, const std::string& error_message = "");

    /**
     * @brief Gets execution history with thread safety.
     * @return Copy of execution history.
     */
    [[nodiscard]] auto getExecutionHistory() const -> std::vector<ExecutionEntry>;

    /**
     * @brief Gets recent execution statistics.
     * @param count Number of recent executions to analyze.
     * @return Pair of (success_count, total_count).
     */
    [[nodiscard]] auto getRecentStats(size_t count = 10) const -> std::pair<size_t, size_t>;

    /**
     * @brief Computes the next time this job is scheduled to run.
     * @param from The instant to search forward from (defaults to now).
     * @return The next execution time, or nullopt if the cron expression is
     *         invalid or never fires.
     */
    [[nodiscard]] auto nextRun(
        std::chrono::system_clock::time_point from =
            std::chrono::system_clock::now()) const
        -> std::optional<std::chrono::system_clock::time_point>;

    /**
     * @brief Computes the next @p count scheduled run times.
     * @param count Maximum number of upcoming times to return.
     * @param from The instant to search forward from (defaults to now).
     * @return Up to @p count execution times (empty if the expression is
     *         invalid or never fires).
     */
    [[nodiscard]] auto nextRuns(
        std::size_t count, std::chrono::system_clock::time_point from =
                               std::chrono::system_clock::now()) const
        -> std::vector<std::chrono::system_clock::time_point>;

    /**
     * @brief Clears execution history to free memory.
     */
    void clearHistory();

    /**
     * @brief Converts the CronJob object to a JSON representation.
     * @return JSON representation of the CronJob object.
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json;

    /**
     * @brief Creates a CronJob object from a JSON representation.
     * @param jsonObj JSON object representing a CronJob.
     * @return CronJob object created from the JSON representation.
     */
    static auto fromJson(const nlohmann::json& jsonObj) -> CronJob;

    /**
     * @brief Comparison operators for sorting and searching.
     */
    bool operator<(const CronJob& other) const noexcept {
        return priority_ > other.priority_; // Higher priority comes first
    }

    bool operator==(const CronJob& other) const noexcept {
        return time_ == other.time_ && command_ == other.command_;
    }
};

#endif // CRON_JOB_HPP

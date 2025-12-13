#pragma once
/*
 * downloader.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-3

Description: Modern C++20 download manager with async support

**************************************************/

#ifndef ATOM_WEB_HTTP_DOWNLOADER_HPP
#define ATOM_WEB_HTTP_DOWNLOADER_HPP

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "atom/type/compat.hpp"

namespace atom::web {

// Use compatibility expected type for cross-compiler support
template <typename T, typename E>
using expected = atom::type::compat::expected<T, E>;
template <typename E>
using unexpected = atom::type::compat::unexpected<E>;

/**
 * @brief Enumeration of download task statuses.
 */
enum class DownloadStatus {
    Pending,      ///< Task is queued but not started
    Downloading,  ///< Task is actively downloading
    Paused,       ///< Task is paused
    Completed,    ///< Task completed successfully
    Failed,       ///< Task failed with an error
    Cancelled,    ///< Task was cancelled by user
    Verifying     ///< Task is verifying checksum
};

/**
 * @brief Convert DownloadStatus to string
 */
[[nodiscard]] constexpr auto downloadStatusToString(
    DownloadStatus status) noexcept -> std::string_view {
    switch (status) {
        case DownloadStatus::Pending:
            return "Pending";
        case DownloadStatus::Downloading:
            return "Downloading";
        case DownloadStatus::Paused:
            return "Paused";
        case DownloadStatus::Completed:
            return "Completed";
        case DownloadStatus::Failed:
            return "Failed";
        case DownloadStatus::Cancelled:
            return "Cancelled";
        case DownloadStatus::Verifying:
            return "Verifying";
        default:
            return "Unknown";
    }
}

/**
 * @brief Download error codes
 */
enum class DownloadError {
    Success = 0,
    NetworkError,
    ConnectionFailed,
    Timeout,
    FileError,
    DiskFull,
    PermissionDenied,
    InvalidUrl,
    NotFound,
    ServerError,
    ChecksumMismatch,
    Cancelled,
    AlreadyExists,
    TaskNotFound,
    InvalidState,
    Unknown
};

/**
 * @brief Convert DownloadError to string
 */
[[nodiscard]] constexpr auto downloadErrorToString(DownloadError error) noexcept
    -> std::string_view {
    switch (error) {
        case DownloadError::Success:
            return "Success";
        case DownloadError::NetworkError:
            return "Network error";
        case DownloadError::ConnectionFailed:
            return "Connection failed";
        case DownloadError::Timeout:
            return "Timeout";
        case DownloadError::FileError:
            return "File error";
        case DownloadError::DiskFull:
            return "Disk full";
        case DownloadError::PermissionDenied:
            return "Permission denied";
        case DownloadError::InvalidUrl:
            return "Invalid URL";
        case DownloadError::NotFound:
            return "Not found";
        case DownloadError::ServerError:
            return "Server error";
        case DownloadError::ChecksumMismatch:
            return "Checksum mismatch";
        case DownloadError::Cancelled:
            return "Cancelled";
        case DownloadError::AlreadyExists:
            return "Already exists";
        case DownloadError::TaskNotFound:
            return "Task not found";
        case DownloadError::InvalidState:
            return "Invalid state";
        case DownloadError::Unknown:
            return "Unknown error";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Checksum algorithm types
 */
enum class ChecksumType { None, MD5, SHA1, SHA256, SHA512 };

/**
 * @brief Download task information structure.
 */
struct DownloadTaskInfo {
    std::string taskId;
    std::string url;
    std::string filePath;
    DownloadStatus status{DownloadStatus::Pending};
    size_t downloadedBytes{0};
    size_t totalBytes{0};
    double progress{0.0};
    double speedBytesPerSec{0.0};
    std::chrono::seconds estimatedTimeRemaining{0};
    std::string errorMessage;
    int retryCount{0};
    int priority{0};
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;

    [[nodiscard]] auto isActive() const noexcept -> bool {
        return status == DownloadStatus::Downloading ||
               status == DownloadStatus::Pending;
    }

    [[nodiscard]] auto isFinished() const noexcept -> bool {
        return status == DownloadStatus::Completed ||
               status == DownloadStatus::Failed ||
               status == DownloadStatus::Cancelled;
    }
};

/**
 * @brief Download manager configuration.
 */
struct DownloadManagerConfig {
    size_t maxConcurrentDownloads{3};
    size_t maxRetries{3};
    std::chrono::seconds retryDelay{5};
    size_t chunkSize{1024 * 1024};  // 1MB
    bool enableResume{true};
    std::string tempDirectory;
    size_t maxDownloadSpeed{0};  // 0 = unlimited
};

/**
 * @class DownloadManager
 * @brief A class that manages download tasks using the Pimpl idiom to hide
 * implementation details.
 */
class DownloadManager {
public:
    /**
     * @brief Constructor.
     * @param task_file The file path to save the download task list.
     * @throws std::runtime_error if initialization fails.
     */
    explicit DownloadManager(const std::string& task_file);

    /**
     * @brief Destructor, releases resources.
     */
    ~DownloadManager();

    DownloadManager(const DownloadManager&) = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;

    DownloadManager(DownloadManager&&) noexcept = default;
    DownloadManager& operator=(DownloadManager&&) noexcept = default;

    /**
     * @brief Adds a download task.
     * @param url The URL of the file to download.
     * @param filePath The local file path to save the downloaded file.
     * @param priority The priority of the download task.
     * @return Task ID for tracking.
     */
    auto addTask(std::string_view url, std::string_view filePath,
                 int priority = 0) -> std::string;

    /**
     * @brief Adds multiple download tasks in batch.
     * @param tasks Span of URL and file path pairs.
     * @param priority The priority for all tasks.
     * @return Vector of task IDs.
     */
    auto addTasks(std::span<const std::pair<std::string, std::string>> tasks,
                  int priority = 0) -> std::vector<std::string>;

    /**
     * @brief Removes a download task by URL.
     * @param url The URL of the task to remove.
     * @return True if task was removed.
     */
    auto removeTask(std::string_view url) -> bool;

    /**
     * @brief Removes a download task by task ID.
     * @param taskId The ID of the task to remove.
     * @return True if task was removed.
     */
    auto removeTaskById(std::string_view taskId) -> bool;

    /**
     * @brief Resumes a paused download task.
     * @param index The index in the task list.
     */
    void resumeTask(size_t index);

    /**
     * @brief Gets the progress of a download task.
     * @param url The URL of the task.
     * @return The progress as a percentage (0-100).
     */
    auto getProgress(std::string_view url) const -> double;

    /**
     * @brief Gets detailed task information.
     * @param url The URL of the task.
     * @return Task info if found.
     */
    auto getTaskInfo(std::string_view url) const
        -> std::optional<DownloadTaskInfo>;

    /**
     * @brief Gets all task information.
     * @return Vector of all task info.
     */
    auto getAllTaskInfo() const -> std::vector<DownloadTaskInfo>;

    /**
     * @brief Gets the number of active downloads.
     * @return Number of active downloads.
     */
    auto getActiveDownloadCount() const -> size_t;

    /**
     * @brief Gets the total download speed across all active downloads.
     * @return Speed in bytes per second.
     */
    auto getTotalDownloadSpeed() const -> double;

    /**
     * @brief Registers a callback for when a download completes.
     * @param callback The callback function (url, filePath).
     */
    void onDownloadComplete(
        std::function<void(const std::string&, const std::string&)> callback);

    /**
     * @brief Registers a callback for download progress updates.
     * @param callback The callback function (url, progress, speed, eta).
     */
    void onProgressUpdate(std::function<void(const std::string&, double, double,
                                             std::chrono::seconds)>
                              callback);

    /**
     * @brief Registers a callback for download errors.
     * @param callback The callback function (url, errorMessage).
     */
    void onError(
        std::function<void(const std::string&, const std::string&)> callback);

    /**
     * @brief Registers a callback for download status changes.
     * @param callback The callback function (url, oldStatus, newStatus).
     */
    void onStatusChange(
        std::function<void(const std::string&, DownloadStatus, DownloadStatus)>
            callback);

    /**
     * @brief Gets the number of active download tasks.
     * @return Number of tasks currently being downloaded.
     */
    size_t getActiveTaskCount() const;

    /**
     * @brief Gets the total number of tasks.
     * @return Total number of tasks in the manager.
     */
    size_t getTotalTaskCount() const;

    /**
     * @brief Checks if the download manager is currently running.
     * @return True if running, false otherwise.
     */
    bool isRunning() const;

    /**
     * @brief Pauses a download task.
     * @param url The URL of the task to pause.
     * @return True if task was paused.
     */
    auto pauseTask(std::string_view url) -> bool;

    /**
     * @brief Resumes a paused download task by URL.
     * @param url The URL of the task to resume.
     * @return True if task was resumed.
     */
    auto resumeTaskByUrl(std::string_view url) -> bool;

    /**
     * @brief Cancels a download task.
     * @param url The URL of the task to cancel.
     * @return True if task was cancelled.
     */
    auto cancelTask(std::string_view url) -> bool;

    /**
     * @brief Pause all active downloads.
     */
    void pauseAll();

    /**
     * @brief Resume all paused downloads.
     */
    void resumeAll();

    /**
     * @brief Cancel all downloads.
     */
    void cancelAll();

    /**
     * @brief Saves tasks to a file.
     * @param filePath The file path to save tasks to.
     * @return True if tasks were saved successfully.
     */
    auto saveTasks(std::string_view filePath) const -> bool;

    /**
     * @brief Loads tasks from a file.
     * @param filePath The file path to load tasks from.
     * @return True if tasks were loaded successfully.
     */
    auto loadTasks(std::string_view filePath) -> bool;

    /**
     * @brief Apply configuration settings.
     * @param config The configuration to apply.
     */
    void applyConfig(const DownloadManagerConfig& config);

    /**
     * @brief Get current configuration.
     * @return Current configuration.
     */
    auto getConfig() const -> DownloadManagerConfig;

    /**
     * @brief Wait for all downloads to complete.
     * @param timeout Maximum time to wait.
     * @return True if all downloads completed within timeout.
     */
    auto waitForCompletion(
        std::chrono::seconds timeout = std::chrono::seconds::max()) -> bool;

    /**
     * @brief Check if a URL is already in the download queue.
     * @param url The URL to check.
     * @return True if URL is in queue.
     */
    auto hasTask(std::string_view url) const -> bool;

    /**
     * @brief Start the download manager.
     */
    void start();

    /**
     * @brief Stop the download manager.
     */
    void stop();

    /**
     * @brief Set the number of concurrent download threads.
     * @param count Number of threads.
     */
    void setThreadCount(size_t count);

    /**
     * @brief Set the maximum number of retries for failed downloads.
     * @param count Maximum retry count.
     */
    void setMaxRetries(size_t count);

    /**
     * @brief Set checksum verification for a task.
     * @param url The URL of the task.
     * @param type Checksum algorithm type.
     * @param expectedChecksum Expected checksum value.
     * @return True if checksum was set.
     */
    auto setChecksum(std::string_view url, ChecksumType type,
                     std::string_view expectedChecksum) -> bool;

    /**
     * @brief Set priority for a task.
     * @param url The URL of the task.
     * @param priority New priority value.
     * @return True if priority was set.
     */
    auto setPriority(std::string_view url, int priority) -> bool;

    /**
     * @brief Get tasks by status.
     * @param status The status to filter by.
     * @return Vector of matching task info.
     */
    [[nodiscard]] auto getTasksByStatus(DownloadStatus status) const
        -> std::vector<DownloadTaskInfo>;

    /**
     * @brief Clear completed tasks from the list.
     * @return Number of tasks cleared.
     */
    auto clearCompletedTasks() -> size_t;

    /**
     * @brief Clear failed tasks from the list.
     * @return Number of tasks cleared.
     */
    auto clearFailedTasks() -> size_t;

    /**
     * @brief Retry all failed tasks.
     * @return Number of tasks retried.
     */
    auto retryFailedTasks() -> size_t;

    /**
     * @brief Download a file synchronously (blocking).
     * @param url The URL to download.
     * @param filePath The local file path.
     * @param progressCallback Optional progress callback.
     * @return Expected containing downloaded bytes or error.
     */
    [[nodiscard]] static auto downloadSync(
        std::string_view url, std::string_view filePath,
        std::function<void(size_t, size_t)> progressCallback = nullptr)
        -> expected<size_t, DownloadError>;

    /**
     * @brief Download a file asynchronously.
     * @param url The URL to download.
     * @param filePath The local file path.
     * @param progressCallback Optional progress callback.
     * @param stopToken Optional stop token for cancellation.
     * @return Future containing expected downloaded bytes or error.
     */
    [[nodiscard]] static auto downloadAsync(
        std::string_view url, std::string_view filePath,
        std::function<void(size_t, size_t)> progressCallback = nullptr,
        std::stop_token stopToken = {})
        -> std::future<expected<size_t, DownloadError>>;

    /**
     * @brief Download multiple files in parallel.
     * @param downloads Vector of (url, filePath) pairs.
     * @param maxConcurrent Maximum concurrent downloads.
     * @return Vector of results (task index, expected bytes or error).
     */
    [[nodiscard]] static auto downloadBatch(
        std::span<const std::pair<std::string, std::string>> downloads,
        size_t maxConcurrent = 3)
        -> std::vector<expected<size_t, DownloadError>>;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Simple download function for one-off downloads.
 * @param url The URL to download.
 * @param filePath The local file path.
 * @param timeout Download timeout.
 * @return Expected containing downloaded bytes or error.
 */
[[nodiscard]] auto download(std::string_view url, std::string_view filePath,
                            std::chrono::seconds timeout = std::chrono::seconds{
                                300}) -> expected<size_t, DownloadError>;

/**
 * @brief Download to memory instead of file.
 * @param url The URL to download.
 * @param timeout Download timeout.
 * @return Expected containing downloaded data or error.
 */
[[nodiscard]] auto downloadToMemory(
    std::string_view url,
    std::chrono::seconds timeout = std::chrono::seconds{
        300}) -> expected<std::vector<std::byte>, DownloadError>;

}  // namespace atom::web

#endif  // ATOM_WEB_HTTP_DOWNLOADER_HPP

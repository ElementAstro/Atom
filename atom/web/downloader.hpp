#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>

#ifdef USE_ASIO
#include <asio.hpp>
#endif

namespace atom::web {

/**
 * @class DownloadManager
 * @brief A class that manages download tasks using the Pimpl idiom to hide implementation details.
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
     * @param url The download URL.
     * @param filepath The path to save the downloaded file.
     * @param priority The priority of the download task, higher values indicate higher priority.
     * @throws std::invalid_argument if URL or filepath is empty.
     */
    void addTask(const std::string& url, const std::string& filepath, int priority = 0);

    /**
     * @brief Removes a download task.
     * @param index The index in the task list.
     * @return True if the task is successfully removed, false otherwise.
     */
    bool removeTask(size_t index);

    /**
     * @brief Starts the download tasks.
     * @param thread_count The number of download threads, default is the number of CPU cores.
     * @param download_speed The download speed limit (bytes per second), 0 means no limit.
     * @throws std::runtime_error if starting fails.
     */
    void start(size_t thread_count = std::thread::hardware_concurrency(), size_t download_speed = 0);

    /**
     * @brief Stops all download tasks gracefully.
     */
    void stop();

    /**
     * @brief Pauses a download task.
     * @param index The index in the task list.
     */
    void pauseTask(size_t index);

    /**
     * @brief Resumes a paused download task.
     * @param index The index in the task list.
     */
    void resumeTask(size_t index);

    /**
     * @brief Gets the number of bytes downloaded for a task.
     * @param index The index in the task list.
     * @return The number of bytes downloaded.
     */
    size_t getDownloadedBytes(size_t index) const;

    /**
     * @brief Gets the total size of a download task.
     * @param index The index in the task list.
     * @return The total size in bytes, 0 if unknown.
     */
    size_t getTotalBytes(size_t index) const;

    /**
     * @brief Gets the download progress as a percentage.
     * @param index The index in the task list.
     * @return Progress percentage (0.0 to 100.0), -1.0 if unknown.
     */
    double getProgress(size_t index) const;

    /**
     * @brief Cancels a download task.
     * @param index The index in the task list.
     */
    void cancelTask(size_t index);

    /**
     * @brief Dynamically adjusts the number of download threads.
     * @param thread_count The new number of download threads.
     */
    void setThreadCount(size_t thread_count);

    /**
     * @brief Sets the maximum number of retries for download errors.
     * @param retries The maximum number of retries for each task on failure.
     */
    void setMaxRetries(size_t retries);

    /**
     * @brief Registers a callback function to be called when a download is complete.
     * @param callback The callback function, with the task index and success status as parameters.
     */
    void onDownloadComplete(const std::function<void(size_t, bool)>& callback);

    /**
     * @brief Registers a callback function to be called when the download progress is updated.
     * @param callback The callback function, with the task index and download percentage as parameters.
     */
    void onProgressUpdate(const std::function<void(size_t, double)>& callback);

    /**
     * @brief Registers a callback function to be called when an error occurs.
     * @param callback The callback function, with the task index and error message as parameters.
     */
    void onError(const std::function<void(size_t, const std::string&)>& callback);

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

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace atom::web
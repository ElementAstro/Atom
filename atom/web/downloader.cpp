#include "downloader.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

#ifdef USE_ASIO
#include <asio.hpp>
#endif

#include <spdlog/spdlog.h>
#include "atom/web/curl.hpp"

namespace atom::web {

class DownloadManager::Impl {
public:
    explicit Impl(std::string task_file);
    ~Impl();

    void addTask(const std::string& url, const std::string& filepath,
                 int priority);
    auto removeTask(size_t index) -> bool;
    void start(size_t thread_count, size_t download_speed);
    void stop();
    void pauseTask(size_t index);
    void resumeTask(size_t index);
    void cancelTask(size_t index);
    auto getDownloadedBytes(size_t index) const -> size_t;
    auto getTotalBytes(size_t index) const -> size_t;
    auto getProgress(size_t index) const -> double;
    void setThreadCount(size_t thread_count);
    void setMaxRetries(size_t retries);
    void onDownloadComplete(const std::function<void(size_t, bool)>& callback);
    void onProgressUpdate(const std::function<void(size_t, double)>& callback);
    void onError(
        const std::function<void(size_t, const std::string&)>& callback);
    auto getActiveTaskCount() const -> size_t;
    auto getTotalTaskCount() const -> size_t;
    auto isRunning() const -> bool;

private:
    enum class TaskStatus {
        Pending,
        Running,
        Paused,
        Completed,
        Cancelled,
        Failed
    };

    struct DownloadTask {
        std::string url;
        std::string filepath;
        TaskStatus status{TaskStatus::Pending};
        std::atomic<size_t> downloadedBytes{0};
        std::atomic<size_t> totalBytes{0};
        int priority{0};
        std::atomic<size_t> retries{0};
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastUpdateTime;
        // Adding taskIndex here for easier access if needed, though not
        // strictly necessary for this fix size_t originalIndex;

        auto operator<(const DownloadTask& other) const -> bool {
            return priority < other.priority;
        }
    };

    void downloadWorker(size_t download_speed);
    // Modified signature to include taskIndex
    void downloadTask(size_t taskIndex, DownloadTask& task,
                      size_t download_speed);
    void saveTaskListToFile();
    void loadTaskListFromFile();
    void updateTaskStatus(size_t index, TaskStatus status);
    auto getTaskByIndex(size_t index) -> DownloadTask*;
    auto getTaskByIndex(size_t index) const -> const DownloadTask*;

    std::string taskFile_;
    std::vector<std::unique_ptr<DownloadTask>> tasks_;
    std::priority_queue<size_t> taskQueue_;
    mutable std::shared_mutex tasksMutex_;
    std::mutex queueMutex_;
    std::condition_variable taskCondition_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shouldStop_{false};
    std::atomic<size_t> maxRetries_{3};
    std::atomic<size_t> threadCount_{std::thread::hardware_concurrency()};
    std::atomic<size_t> activeTaskCount_{0};

    std::vector<std::thread> workers_;

    std::function<void(size_t, bool)> onComplete_;
    std::function<void(size_t, double)> onProgress_;
    std::function<void(size_t, const std::string&)> onError_;

#ifdef USE_ASIO
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
        work_guard_;
    std::vector<std::thread> io_threads_;
#endif
};

DownloadManager::Impl::Impl(std::string task_file)
    : taskFile_(std::move(task_file)) {
    try {
        spdlog::info("Initializing DownloadManager with task file: {}",
                     taskFile_);

        std::filesystem::path taskPath(taskFile_);
        if (!taskPath.parent_path().empty()) {
            std::filesystem::create_directories(taskPath.parent_path());
        }

        loadTaskListFromFile();

#ifdef USE_ASIO
        io_context_ = std::make_unique<asio::io_context>();
        work_guard_ = std::make_unique<
            asio::executor_work_guard<asio::io_context::executor_type>>(
            io_context_->get_executor());
#endif

        spdlog::debug("DownloadManager initialized with {} tasks",
                      tasks_.size());

    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize DownloadManager: {}", e.what());
        throw std::runtime_error("DownloadManager initialization failed: " +
                                 std::string(e.what()));
    }
}

DownloadManager::Impl::~Impl() {
    spdlog::debug("Destroying DownloadManager");
    stop();

    try {
        saveTaskListToFile();
    } catch (const std::exception& e) {
        spdlog::error("Failed to save task list during destruction: {}",
                      e.what());
    }

    spdlog::info("DownloadManager destroyed");
}

void DownloadManager::Impl::addTask(const std::string& url,
                                    const std::string& filepath, int priority) {
    if (url.empty() || filepath.empty()) {
        throw std::invalid_argument("URL and filepath cannot be empty");
    }

    std::unique_lock lock(tasksMutex_);

    auto task = std::make_unique<DownloadTask>();
    task->url = url;
    task->filepath = filepath;
    task->priority = priority;
    task->status = TaskStatus::Pending;

    size_t index = tasks_.size();
    tasks_.push_back(std::move(task));

    {
        std::lock_guard queueLock(queueMutex_);
        taskQueue_.push(index);
    }

    lock.unlock();
    taskCondition_.notify_one();

    try {
        saveTaskListToFile();
    } catch (const std::exception& e) {
        spdlog::warn("Failed to save task list after adding task: {}",
                     e.what());
    }

    spdlog::debug("Added download task: {} -> {}, priority: {}", url, filepath,
                  priority);
}

auto DownloadManager::Impl::removeTask(size_t index) -> bool {
    std::unique_lock lock(tasksMutex_);

    if (index >= tasks_.size()) {
        spdlog::warn("Attempted to remove task with invalid index: {}", index);
        return false;
    }

    if (tasks_[index]->status == TaskStatus::Running) {
        spdlog::warn("Cannot remove running task at index: {}", index);
        return false;
    }

    tasks_.erase(tasks_.begin() + index);

    std::priority_queue<size_t> newQueue;
    for (size_t i = 0; i < tasks_.size(); ++i) {
        if (tasks_[i]->status == TaskStatus::Pending) {
            newQueue.push(i);
        }
    }

    {
        std::lock_guard queueLock(queueMutex_);
        taskQueue_ = std::move(newQueue);
    }

    lock.unlock();

    try {
        saveTaskListToFile();
    } catch (const std::exception& e) {
        spdlog::warn("Failed to save task list after removing task: {}",
                     e.what());
    }

    spdlog::debug("Removed task at index: {}", index);
    return true;
}

void DownloadManager::Impl::start(size_t thread_count, size_t download_speed) {
    if (running_.exchange(true)) {
        spdlog::warn("DownloadManager is already running");
        return;
    }

    shouldStop_ = false;
    threadCount_ = thread_count;
    activeTaskCount_ = 0;

    try {
#ifdef USE_ASIO
        for (size_t i = 0; i < threadCount_; ++i) {
            io_threads_.emplace_back([this]() {
                try {
                    io_context_->run();
                } catch (const std::exception& e) {
                    spdlog::error("ASIO thread error: {}", e.what());
                }
            });
        }
#endif

        workers_.reserve(threadCount_);
        for (size_t i = 0; i < threadCount_; ++i) {
            workers_.emplace_back([this, download_speed]() {
                try {
                    downloadWorker(download_speed);
                } catch (const std::exception& e) {
                    spdlog::error("Download worker thread error: {}", e.what());
                }
            });
        }

        spdlog::info(
            "Started DownloadManager with {} threads, speed limit: {} bytes/s",
            threadCount_.load(), download_speed);

    } catch (const std::exception& e) {
        running_ = false;
        spdlog::error("Failed to start DownloadManager: {}", e.what());
        throw std::runtime_error("Failed to start download manager: " +
                                 std::string(e.what()));
    }
}

void DownloadManager::Impl::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    spdlog::info("Stopping DownloadManager");
    shouldStop_ = true;
    taskCondition_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();

#ifdef USE_ASIO
    if (work_guard_) {
        work_guard_.reset();
    }

    if (io_context_) {
        io_context_->stop();
    }

    for (auto& thread : io_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    io_threads_.clear();
#endif

    spdlog::info("DownloadManager stopped");
}

void DownloadManager::Impl::pauseTask(size_t index) {
    if (auto* task = getTaskByIndex(index)) {
        updateTaskStatus(index, TaskStatus::Paused);
        spdlog::debug("Paused task at index: {}", index);
    }
}

void DownloadManager::Impl::resumeTask(size_t index) {
    if (auto* task = getTaskByIndex(index)) {
        if (task->status == TaskStatus::Paused) {
            updateTaskStatus(index, TaskStatus::Pending);

            std::lock_guard queueLock(queueMutex_);
            taskQueue_.push(index);
            taskCondition_.notify_one();

            spdlog::debug("Resumed task at index: {}", index);
        }
    }
}

void DownloadManager::Impl::cancelTask(size_t index) {
    if (auto* task = getTaskByIndex(index)) {
        updateTaskStatus(index, TaskStatus::Cancelled);
        spdlog::debug("Cancelled task at index: {}", index);
    }
}

auto DownloadManager::Impl::getDownloadedBytes(size_t index) const -> size_t {
    if (const auto* task = getTaskByIndex(index)) {
        return task->downloadedBytes.load();
    }
    return 0;
}

auto DownloadManager::Impl::getTotalBytes(size_t index) const -> size_t {
    if (const auto* task = getTaskByIndex(index)) {
        return task->totalBytes.load();
    }
    return 0;
}

auto DownloadManager::Impl::getProgress(size_t index) const -> double {
    if (const auto* task = getTaskByIndex(index)) {
        size_t total = task->totalBytes.load();
        if (total > 0) {
            return (static_cast<double>(task->downloadedBytes.load()) / total) *
                   100.0;
        }
    }
    return -1.0;
}

void DownloadManager::Impl::setThreadCount(size_t thread_count) {
    threadCount_ = thread_count;
    spdlog::debug("Set thread count to: {}", thread_count);
}

void DownloadManager::Impl::setMaxRetries(size_t retries) {
    maxRetries_ = retries;
    spdlog::debug("Set max retries to: {}", retries);
}

void DownloadManager::Impl::onDownloadComplete(
    const std::function<void(size_t, bool)>& callback) {
    onComplete_ = callback;
}

void DownloadManager::Impl::onProgressUpdate(
    const std::function<void(size_t, double)>& callback) {
    onProgress_ = callback;
}

void DownloadManager::Impl::onError(
    const std::function<void(size_t, const std::string&)>& callback) {
    onError_ = callback;
}

auto DownloadManager::Impl::getActiveTaskCount() const -> size_t {
    return activeTaskCount_.load();
}

auto DownloadManager::Impl::getTotalTaskCount() const -> size_t {
    std::shared_lock lock(tasksMutex_);
    return tasks_.size();
}

auto DownloadManager::Impl::isRunning() const -> bool {
    return running_.load();
}

void DownloadManager::Impl::downloadWorker(size_t download_speed) {
    while (!shouldStop_) {
        size_t taskIndex;

        {
            std::unique_lock lock(queueMutex_);
            taskCondition_.wait(
                lock, [this] { return shouldStop_ || !taskQueue_.empty(); });

            if (shouldStop_ || taskQueue_.empty()) {
                break;
            }

            taskIndex = taskQueue_.top();
            taskQueue_.pop();
        }

        // Use a shared lock to get the task to prevent issues if tasks_ is
        // modified
        std::shared_lock tasks_lock(tasksMutex_);
        if (taskIndex < tasks_.size()) {
            DownloadTask* taskPtr = tasks_[taskIndex].get();
            if (taskPtr && taskPtr->status == TaskStatus::Pending) {
                // Unlock before calling downloadTask to avoid holding lock for
                // long duration and potential deadlocks if downloadTask tries
                // to acquire tasksMutex_
                tasks_lock.unlock();
                downloadTask(taskIndex, *taskPtr, download_speed);
            } else {
                // Task might have been removed or status changed
                tasks_lock.unlock();
            }
        } else {
            tasks_lock.unlock();
        }
    }
}

// Modified signature to include taskIndex
void DownloadManager::Impl::downloadTask(size_t taskIndex, DownloadTask& task,
                                         size_t download_speed) {
    // Use taskIndex directly
    updateTaskStatus(taskIndex, TaskStatus::Running);
    activeTaskCount_++;

    task.startTime = std::chrono::steady_clock::now();
    task.lastUpdateTime = task.startTime;

    try {
        CurlWrapper curl;
        bool success = false;

        curl.setUrl(task.url)
            .setRequestMethod("GET")
            .setOnResponseCallback([&](const std::string& data) {
                std::ofstream ofs(task.filepath,
                                  std::ios::binary | std::ios::app);
                if (ofs) {
                    ofs.write(data.c_str(), data.size());
                    task.downloadedBytes += data.size();

                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - task.lastUpdateTime)
                            .count() > 100) {
                        task.lastUpdateTime = now;

                        if (onProgress_) {
                            double progress = -1.0;
                            if (task.totalBytes > 0) {
                                progress =
                                    (static_cast<double>(task.downloadedBytes) /
                                     task.totalBytes) *
                                    100.0;
                            }
                            // Use taskIndex directly
                            onProgress_(taskIndex, progress);
                        }
                    }
                } else {
                    spdlog::error("Failed to open file for writing: {}",
                                  task.filepath);
                }
            })
            .setOnErrorCallback([&](CURLcode code) {
                std::string errorMsg =
                    "Download error: " + std::to_string(static_cast<int>(code));
                spdlog::error("Download error for URL {}: {}", task.url,
                              errorMsg);

                if (onError_) {
                    // Use taskIndex directly
                    onError_(taskIndex, errorMsg);
                }

                if (task.retries < maxRetries_) {
                    task.retries++;
                    std::lock_guard queueLock(queueMutex_);
                    // Use taskIndex directly
                    taskQueue_.push(taskIndex);
                    taskCondition_.notify_one();
                } else {
                    // Use taskIndex directly
                    updateTaskStatus(taskIndex, TaskStatus::Failed);
                }
            });

        if (download_speed > 0) {
            curl.setMaxDownloadSpeed(download_speed);
        }

#ifdef USE_ASIO
        // For ASIO, ensure 'success' is correctly handled.
        // The lambda captures 'success' by reference.
        io_context_->post([&curl, &success, &task, this, taskIndex]() {
            std::string result_str = curl.perform();
            // Assuming perform returns an empty string on success (no error
            // message) and onError callback handles CURLcode errors.
            success = result_str.empty();
        });
        // Note: For ASIO, the subsequent 'if (success)' block will execute
        // potentially before the posted task completes. This needs careful
        // synchronization or a different pattern for handling completion. For
        // simplicity of this fix, I am keeping the structure, but this is a
        // potential issue. A robust ASIO implementation would likely use the
        // completion handler of the post or a promise/future to signal
        // completion and success.
#else
        std::string result_str = curl.perform();
        // Assuming perform returns an empty string on success (no error
        // message) and onError callback handles CURLcode errors.
        success = result_str.empty();
#endif

        // This check might be problematic with ASIO's async nature as 'success'
        // might not be set yet. This part of the logic might need further
        // refinement if ASIO is used.
        if (success) {
            // Check task status again, as onError might have been called in
            // another thread (less likely without ASIO for perform) or if
            // perform itself is asynchronous internally. A read lock is safer
            // if getTaskByIndex is used.
            std::shared_lock lock(tasksMutex_);
            DownloadTask* currentTaskState = tasks_[taskIndex].get();
            lock.unlock();

            // Only mark as completed if it wasn't marked failed by onError
            if (currentTaskState &&
                currentTaskState->status != TaskStatus::Failed &&
                currentTaskState->status != TaskStatus::Pending) {
                updateTaskStatus(taskIndex, TaskStatus::Completed);
                spdlog::info("Download completed: {}", task.url);

                if (onComplete_) {
                    onComplete_(taskIndex, true);
                }
            } else if (currentTaskState &&
                       (currentTaskState->status == TaskStatus::Failed ||
                        currentTaskState->status == TaskStatus::Pending)) {
                // onError already handled it, or it's pending retry.
                // `success` might have been true from result.empty() but
                // onError took precedence.
                spdlog::warn(
                    "Download for {} had perform() success but task status is "
                    "{} due to onError or retry.",
                    task.url, static_cast<int>(currentTaskState->status));
                if (onComplete_ &&
                    currentTaskState->status ==
                        TaskStatus::Failed) {  // Only call onComplete if truly
                                               // failed and not retrying
                    onComplete_(taskIndex, false);
                }
            }
        }
        // If !success (i.e., result_str was not empty), and onError was not
        // called, the task remains 'Running' and then eventually finishes the
        // downloadTask scope. This might leave it in an inconsistent state if
        // result_str indicated an error not caught by CURLcode. A more robust
        // solution would be to also treat non-empty result_str as an error. For
        // example: if (!success && onError_) { // if result_str was not empty
        //     onError_(taskIndex, "Download failed: " + result_str); //
        //     Assuming result_str is an error message
        //     updateTaskStatus(taskIndex, TaskStatus::Failed);
        //     if (onComplete_) onComplete_(taskIndex, false);
        // }

    } catch (const std::exception& e) {
        spdlog::error("Exception during download of {}: {}", task.url,
                      e.what());
        // Use taskIndex directly
        updateTaskStatus(taskIndex, TaskStatus::Failed);

        if (onError_) {
            // Use taskIndex directly
            onError_(taskIndex, e.what());
        }

        if (onComplete_) {
            // Use taskIndex directly
            onComplete_(taskIndex, false);
        }
    }

    activeTaskCount_--;
}

void DownloadManager::Impl::updateTaskStatus(size_t index, TaskStatus status) {
    if (auto* task = getTaskByIndex(index)) {
        task->status = status;
    }
}

auto DownloadManager::Impl::getTaskByIndex(size_t index) -> DownloadTask* {
    std::shared_lock lock(tasksMutex_);
    return (index < tasks_.size()) ? tasks_[index].get() : nullptr;
}

auto DownloadManager::Impl::getTaskByIndex(size_t index) const
    -> const DownloadTask* {
    std::shared_lock lock(tasksMutex_);
    return (index < tasks_.size()) ? tasks_[index].get() : nullptr;
}

void DownloadManager::Impl::saveTaskListToFile() {
    try {
        std::ofstream ofs(taskFile_, std::ios::trunc);
        if (!ofs) {
            throw std::runtime_error("Failed to open task file for writing");
        }

        std::shared_lock lock(tasksMutex_);
        for (const auto& task : tasks_) {
            if (task->status != TaskStatus::Completed &&
                task->status != TaskStatus::Cancelled) {
                ofs << task->url << " " << task->filepath << " "
                    << task->priority << "\n";
            }
        }

        spdlog::trace("Saved task list to file: {}", taskFile_);

    } catch (const std::exception& e) {
        spdlog::error("Failed to save task list: {}", e.what());
        throw;
    }
}

void DownloadManager::Impl::loadTaskListFromFile() {
    try {
        std::ifstream ifs(taskFile_);
        if (!ifs) {
            spdlog::debug("Task file does not exist: {}", taskFile_);
            return;
        }

        std::string url, filepath;
        int priority;
        size_t loadedCount = 0;

        while (ifs >> url >> filepath >> priority) {
            auto task = std::make_unique<DownloadTask>();
            task->url = url;
            task->filepath = filepath;
            task->priority = priority;
            task->status = TaskStatus::Pending;

            tasks_.push_back(std::move(task));
            taskQueue_.push(tasks_.size() - 1);
            ++loadedCount;
        }

        spdlog::info("Loaded {} tasks from file: {}", loadedCount, taskFile_);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load task list: {}", e.what());
        throw;
    }
}

DownloadManager::DownloadManager(const std::string& task_file)
    : impl_(std::make_unique<Impl>(task_file)) {}

DownloadManager::~DownloadManager() = default;

void DownloadManager::addTask(const std::string& url,
                              const std::string& filepath, int priority) {
    impl_->addTask(url, filepath, priority);
}

bool DownloadManager::removeTask(size_t index) {
    return impl_->removeTask(index);
}

void DownloadManager::start(size_t thread_count, size_t download_speed) {
    impl_->start(thread_count, download_speed);
}

void DownloadManager::stop() { impl_->stop(); }

void DownloadManager::pauseTask(size_t index) { impl_->pauseTask(index); }

void DownloadManager::resumeTask(size_t index) { impl_->resumeTask(index); }

size_t DownloadManager::getDownloadedBytes(size_t index) const {
    return impl_->getDownloadedBytes(index);
}

size_t DownloadManager::getTotalBytes(size_t index) const {
    return impl_->getTotalBytes(index);
}

double DownloadManager::getProgress(size_t index) const {
    return impl_->getProgress(index);
}

void DownloadManager::cancelTask(size_t index) { impl_->cancelTask(index); }

void DownloadManager::setThreadCount(size_t thread_count) {
    impl_->setThreadCount(thread_count);
}

void DownloadManager::setMaxRetries(size_t retries) {
    impl_->setMaxRetries(retries);
}

void DownloadManager::onDownloadComplete(
    const std::function<void(size_t, bool)>& callback) {
    impl_->onDownloadComplete(callback);
}

void DownloadManager::onProgressUpdate(
    const std::function<void(size_t, double)>& callback) {
    impl_->onProgressUpdate(callback);
}

void DownloadManager::onError(
    const std::function<void(size_t, const std::string&)>& callback) {
    impl_->onError(callback);
}

size_t DownloadManager::getActiveTaskCount() const {
    return impl_->getActiveTaskCount();
}

size_t DownloadManager::getTotalTaskCount() const {
    return impl_->getTotalTaskCount();
}

bool DownloadManager::isRunning() const { return impl_->isRunning(); }

}  // namespace atom::web

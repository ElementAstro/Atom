#include "downloader.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
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
#include "curl.hpp"

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

    auto findTaskIndexByUrl(std::string_view url) const
        -> std::optional<size_t>;
    auto getTaskInfoByIndex(size_t index) const
        -> std::optional<DownloadTaskInfo>;
    auto getAllTaskInfo() const -> std::vector<DownloadTaskInfo>;
    void setConfig(const DownloadManagerConfig& config);
    auto getConfig() const -> DownloadManagerConfig;
    auto saveTasks(std::string_view filePath) const -> bool;
    auto loadTasks(std::string_view filePath) -> bool;

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

    DownloadManagerConfig config_{};

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
    if ([[maybe_unused]] auto* task = getTaskByIndex(index)) {
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
    if ([[maybe_unused]] auto* task = getTaskByIndex(index)) {
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
        auto progressCb = [&](size_t dlNow, size_t dlTotal) {
            task.downloadedBytes = dlNow;
            task.totalBytes = dlTotal;
            if (onProgress_) {
                double progress = -1.0;
                if (dlTotal > 0) {
                    progress = (static_cast<double>(dlNow) / dlTotal) * 100.0;
                }
                onProgress_(taskIndex, progress);
            }
        };

        auto result =
            CurlWrapper::downloadFile(task.url, task.filepath, progressCb, {});
        if (result) {
            updateTaskStatus(taskIndex, TaskStatus::Completed);
            spdlog::info("Download completed: {}", task.url);
            if (onComplete_) {
                onComplete_(taskIndex, true);
            }
        } else {
            std::string errorMsg = "Download failed";
            if (onError_) {
                onError_(taskIndex, errorMsg);
            }

            if (task.retries < maxRetries_) {
                task.retries++;
                std::lock_guard queueLock(queueMutex_);
                taskQueue_.push(taskIndex);
                taskCondition_.notify_one();
            } else {
                updateTaskStatus(taskIndex, TaskStatus::Failed);
                if (onComplete_) {
                    onComplete_(taskIndex, false);
                }
            }
        }
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

auto DownloadManager::Impl::findTaskIndexByUrl(std::string_view url) const
    -> std::optional<size_t> {
    std::shared_lock lock(tasksMutex_);
    for (size_t i = 0; i < tasks_.size(); ++i) {
        if (tasks_[i] && tasks_[i]->url == url) {
            return i;
        }
    }
    return std::nullopt;
}

auto DownloadManager::Impl::getTaskInfoByIndex(size_t index) const
    -> std::optional<DownloadTaskInfo> {
    std::shared_lock lock(tasksMutex_);
    if (index >= tasks_.size() || !tasks_[index]) {
        return std::nullopt;
    }
    const auto& task = *tasks_[index];

    DownloadTaskInfo info;
    info.taskId = task.url;
    info.url = task.url;
    info.filePath = task.filepath;
    switch (task.status) {
        case TaskStatus::Pending:
            info.status = DownloadStatus::Pending;
            break;
        case TaskStatus::Running:
            info.status = DownloadStatus::Downloading;
            break;
        case TaskStatus::Paused:
            info.status = DownloadStatus::Paused;
            break;
        case TaskStatus::Completed:
            info.status = DownloadStatus::Completed;
            break;
        case TaskStatus::Cancelled:
            info.status = DownloadStatus::Cancelled;
            break;
        case TaskStatus::Failed:
            info.status = DownloadStatus::Failed;
            break;
        default:
            info.status = DownloadStatus::Failed;
            break;
    }
    info.downloadedBytes = task.downloadedBytes.load();
    info.totalBytes = task.totalBytes.load();
    if (info.totalBytes > 0) {
        info.progress =
            (static_cast<double>(info.downloadedBytes) / info.totalBytes) *
            100.0;
    } else {
        info.progress = 0.0;
    }
    info.retryCount = static_cast<int>(task.retries.load());
    info.priority = task.priority;
    return info;
}

auto DownloadManager::Impl::getAllTaskInfo() const
    -> std::vector<DownloadTaskInfo> {
    std::vector<DownloadTaskInfo> result;
    std::shared_lock lock(tasksMutex_);
    result.reserve(tasks_.size());
    for (size_t i = 0; i < tasks_.size(); ++i) {
        if (!tasks_[i]) {
            continue;
        }
        const auto& task = *tasks_[i];

        DownloadTaskInfo info;
        info.taskId = task.url;
        info.url = task.url;
        info.filePath = task.filepath;
        switch (task.status) {
            case TaskStatus::Pending:
                info.status = DownloadStatus::Pending;
                break;
            case TaskStatus::Running:
                info.status = DownloadStatus::Downloading;
                break;
            case TaskStatus::Paused:
                info.status = DownloadStatus::Paused;
                break;
            case TaskStatus::Completed:
                info.status = DownloadStatus::Completed;
                break;
            case TaskStatus::Cancelled:
                info.status = DownloadStatus::Cancelled;
                break;
            case TaskStatus::Failed:
                info.status = DownloadStatus::Failed;
                break;
            default:
                info.status = DownloadStatus::Failed;
                break;
        }
        info.downloadedBytes = task.downloadedBytes.load();
        info.totalBytes = task.totalBytes.load();
        if (info.totalBytes > 0) {
            info.progress =
                (static_cast<double>(info.downloadedBytes) / info.totalBytes) *
                100.0;
        }
        info.retryCount = static_cast<int>(task.retries.load());
        info.priority = task.priority;

        result.push_back(std::move(info));
    }
    return result;
}

void DownloadManager::Impl::setConfig(const DownloadManagerConfig& config) {
    config_ = config;
    setThreadCount(config.maxConcurrentDownloads);
    setMaxRetries(config.maxRetries);
}

auto DownloadManager::Impl::getConfig() const -> DownloadManagerConfig {
    return config_;
}

auto DownloadManager::Impl::saveTasks(std::string_view filePath) const -> bool {
    try {
        std::ofstream ofs{std::string(filePath), std::ios::trunc};
        if (!ofs) {
            return false;
        }
        std::shared_lock lock(tasksMutex_);
        for (const auto& task : tasks_) {
            if (task) {
                ofs << task->url << " " << task->filepath << " "
                    << task->priority << "\n";
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

auto DownloadManager::Impl::loadTasks(std::string_view filePath) -> bool {
    try {
        std::ifstream ifs{std::string(filePath)};
        if (!ifs) {
            return false;
        }
        std::unique_lock lock(tasksMutex_);
        tasks_.clear();
        while (!taskQueue_.empty()) {
            taskQueue_.pop();
        }

        std::string url;
        std::string filepath;
        int priority;
        while (ifs >> url >> filepath >> priority) {
            auto task = std::make_unique<DownloadTask>();
            task->url = url;
            task->filepath = filepath;
            task->priority = priority;
            task->status = TaskStatus::Pending;
            tasks_.push_back(std::move(task));
            taskQueue_.push(tasks_.size() - 1);
        }

        taskCondition_.notify_all();
        return true;
    } catch (...) {
        return false;
    }
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

auto DownloadManager::addTask(std::string_view url, std::string_view filePath,
                              int priority) -> std::string {
    impl_->addTask(std::string(url), std::string(filePath), priority);
    return std::string(url);
}

auto DownloadManager::addTasks(
    std::span<const std::pair<std::string, std::string>> tasks,
    int priority) -> std::vector<std::string> {
    std::vector<std::string> ids;
    ids.reserve(tasks.size());
    for (const auto& [url, path] : tasks) {
        ids.push_back(addTask(url, path, priority));
    }
    return ids;
}

auto DownloadManager::removeTask(std::string_view url) -> bool {
    auto idx = impl_->findTaskIndexByUrl(url);
    if (!idx) {
        return false;
    }
    return impl_->removeTask(*idx);
}

auto DownloadManager::removeTaskById(std::string_view taskId) -> bool {
    return removeTask(taskId);
}

void DownloadManager::resumeTask(size_t index) { impl_->resumeTask(index); }

auto DownloadManager::getProgress(std::string_view url) const -> double {
    auto idx = impl_->findTaskIndexByUrl(url);
    if (!idx) {
        return -1.0;
    }
    return impl_->getProgress(*idx);
}

auto DownloadManager::getTaskInfo(std::string_view url) const
    -> std::optional<DownloadTaskInfo> {
    auto idx = impl_->findTaskIndexByUrl(url);
    if (!idx) {
        return std::nullopt;
    }
    return impl_->getTaskInfoByIndex(*idx);
}

auto DownloadManager::getAllTaskInfo() const -> std::vector<DownloadTaskInfo> {
    return impl_->getAllTaskInfo();
}

auto DownloadManager::getActiveDownloadCount() const -> size_t {
    return impl_->getActiveTaskCount();
}

auto DownloadManager::getTotalDownloadSpeed() const -> double { return 0.0; }

void DownloadManager::onDownloadComplete(
    std::function<void(const std::string&, const std::string&)> callback) {
    impl_->onDownloadComplete(
        [this, cb = std::move(callback)](size_t index, bool) {
            if (!cb) {
                return;
            }
            if (auto info = impl_->getTaskInfoByIndex(index)) {
                cb(info->url, info->filePath);
            }
        });
}

void DownloadManager::onProgressUpdate(
    std::function<void(const std::string&, double, double,
                       std::chrono::seconds)>
        callback) {
    impl_->onProgressUpdate(
        [this, cb = std::move(callback)](size_t index, double progress) {
            if (!cb) {
                return;
            }
            if (auto info = impl_->getTaskInfoByIndex(index)) {
                cb(info->url, progress, 0.0, std::chrono::seconds{0});
            }
        });
}

void DownloadManager::onError(
    std::function<void(const std::string&, const std::string&)> callback) {
    impl_->onError(
        [this, cb = std::move(callback)](size_t index, const std::string& msg) {
            if (!cb) {
                return;
            }
            if (auto info = impl_->getTaskInfoByIndex(index)) {
                cb(info->url, msg);
            }
        });
}

void DownloadManager::onStatusChange(
    std::function<void(const std::string&, DownloadStatus, DownloadStatus)>) {}

size_t DownloadManager::getActiveTaskCount() const {
    return impl_->getActiveTaskCount();
}

size_t DownloadManager::getTotalTaskCount() const {
    return impl_->getTotalTaskCount();
}

bool DownloadManager::isRunning() const { return impl_->isRunning(); }

auto DownloadManager::pauseTask(std::string_view url) -> bool {
    auto idx = impl_->findTaskIndexByUrl(url);
    if (!idx) {
        return false;
    }
    impl_->pauseTask(*idx);
    return true;
}

auto DownloadManager::resumeTaskByUrl(std::string_view url) -> bool {
    auto idx = impl_->findTaskIndexByUrl(url);
    if (!idx) {
        return false;
    }
    impl_->resumeTask(*idx);
    return true;
}

auto DownloadManager::cancelTask(std::string_view url) -> bool {
    auto idx = impl_->findTaskIndexByUrl(url);
    if (!idx) {
        return false;
    }
    impl_->cancelTask(*idx);
    return true;
}

void DownloadManager::pauseAll() {
    auto infos = impl_->getAllTaskInfo();
    for (const auto& info : infos) {
        (void)pauseTask(info.url);
    }
}

void DownloadManager::resumeAll() {
    auto infos = impl_->getAllTaskInfo();
    for (const auto& info : infos) {
        (void)resumeTaskByUrl(info.url);
    }
}

void DownloadManager::cancelAll() {
    auto infos = impl_->getAllTaskInfo();
    for (const auto& info : infos) {
        (void)cancelTask(info.url);
    }
}

auto DownloadManager::saveTasks(std::string_view filePath) const -> bool {
    return impl_->saveTasks(filePath);
}

auto DownloadManager::loadTasks(std::string_view filePath) -> bool {
    return impl_->loadTasks(filePath);
}

void DownloadManager::applyConfig(const DownloadManagerConfig& config) {
    impl_->setConfig(config);
}

auto DownloadManager::getConfig() const -> DownloadManagerConfig {
    return impl_->getConfig();
}

auto DownloadManager::waitForCompletion(std::chrono::seconds timeout) -> bool {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (impl_->getActiveTaskCount() == 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

auto DownloadManager::hasTask(std::string_view url) const -> bool {
    return impl_->findTaskIndexByUrl(url).has_value();
}

void DownloadManager::start() {
    impl_->start(std::thread::hardware_concurrency(),
                 static_cast<size_t>(impl_->getConfig().maxDownloadSpeed));
}

void DownloadManager::stop() { impl_->stop(); }

void DownloadManager::setThreadCount(size_t count) {
    impl_->setThreadCount(count);
}

void DownloadManager::setMaxRetries(size_t count) {
    impl_->setMaxRetries(count);
}

auto DownloadManager::setChecksum(std::string_view, ChecksumType,
                                  std::string_view) -> bool {
    return false;
}

auto DownloadManager::setPriority(std::string_view, int) -> bool {
    return false;
}

auto DownloadManager::getTasksByStatus(DownloadStatus) const
    -> std::vector<DownloadTaskInfo> {
    return {};
}

auto DownloadManager::clearCompletedTasks() -> size_t { return 0; }

auto DownloadManager::clearFailedTasks() -> size_t { return 0; }

auto DownloadManager::retryFailedTasks() -> size_t { return 0; }

auto DownloadManager::downloadSync(
    std::string_view url, std::string_view filePath,
    std::function<void(size_t, size_t)> progressCallback)
    -> expected<size_t, DownloadError> {
    auto httpRes =
        CurlWrapper::downloadFile(url, filePath, progressCallback, {});
    if (!httpRes) {
        return unexpected(DownloadError::NetworkError);
    }
    return *httpRes;
}

auto DownloadManager::downloadAsync(
    std::string_view url, std::string_view filePath,
    std::function<void(size_t, size_t)> progressCallback,
    std::stop_token stopToken) -> std::future<expected<size_t, DownloadError>> {
    return std::async(std::launch::async,
                      [url = std::string(url), filePath = std::string(filePath),
                       progressCallback = std::move(progressCallback),
                       stopToken]() -> expected<size_t, DownloadError> {
                          if (stopToken.stop_requested()) {
                              return unexpected(DownloadError::Cancelled);
                          }
                          return downloadSync(url, filePath, progressCallback);
                      });
}

auto DownloadManager::downloadBatch(
    std::span<const std::pair<std::string, std::string>> downloads,
    size_t) -> std::vector<expected<size_t, DownloadError>> {
    std::vector<expected<size_t, DownloadError>> results;
    results.reserve(downloads.size());
    for (const auto& [url, path] : downloads) {
        results.push_back(downloadSync(url, path));
    }
    return results;
}

auto download(std::string_view url, std::string_view filePath,
              std::chrono::seconds timeout) -> expected<size_t, DownloadError> {
    RequestConfig cfg;
    cfg.timeout = timeout;
    auto httpRes = CurlWrapper::downloadFile(url, filePath, nullptr, {});
    if (!httpRes) {
        return unexpected(DownloadError::NetworkError);
    }
    return *httpRes;
}

auto downloadToMemory(std::string_view url, std::chrono::seconds timeout)
    -> expected<std::vector<std::byte>, DownloadError> {
    RequestConfig cfg;
    cfg.timeout = timeout;
    auto httpRes = CurlWrapper::get(url, cfg);
    if (!httpRes) {
        return unexpected(DownloadError::NetworkError);
    }
    const auto& body = httpRes->body;
    std::vector<std::byte> out;
    out.resize(body.size());
    std::memcpy(out.data(), body.data(), body.size());
    return out;
}

}  // namespace atom::web

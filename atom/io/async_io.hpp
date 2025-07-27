#ifndef ATOM_IO_ASYNC_IO_HPP
#define ATOM_IO_ASYNC_IO_HPP

#include <atomic>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <fstream>

#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

#include <spdlog/spdlog.h>
#include "atom/async/pool.hpp"

namespace atom::async::io {

// Configuration structure for async I/O operations
struct AsyncIOConfig {
    std::size_t buffer_size = 65536;           // 64KB default buffer
    std::size_t max_concurrent_ops = 100;      // Maximum concurrent operations
    bool enable_caching = true;                // Enable file metadata caching
    bool enable_progress_reporting = false;    // Enable progress callbacks
    bool enable_statistics = true;             // Enable performance statistics
    std::chrono::milliseconds default_timeout{30000}; // 30 seconds default
    std::size_t cache_size_limit = 1000;       // Maximum cache entries
    bool use_memory_mapping = false;           // Use memory mapping for large files
    std::size_t memory_mapping_threshold = 10 * 1024 * 1024; // 10MB threshold
};

// Statistics for async I/O operations
struct AsyncIOStats {
    std::atomic<std::size_t> files_read{0};
    std::atomic<std::size_t> files_written{0};
    std::atomic<std::size_t> bytes_read{0};
    std::atomic<std::size_t> bytes_written{0};
    std::atomic<std::size_t> operations_completed{0};
    std::atomic<std::size_t> operations_failed{0};
    std::atomic<std::size_t> cache_hits{0};
    std::atomic<std::size_t> cache_misses{0};
    std::chrono::steady_clock::time_point start_time;

    void reset() {
        files_read = 0;
        files_written = 0;
        bytes_read = 0;
        bytes_written = 0;
        operations_completed = 0;
        operations_failed = 0;
        cache_hits = 0;
        cache_misses = 0;
        start_time = std::chrono::steady_clock::now();
    }

    double getOperationsPerSecond() const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        if (duration.count() > 0) {
            return static_cast<double>(operations_completed.load()) / duration.count();
        }
        return 0.0;
    }
};

// Progress callback type
using ProgressCallback = std::function<void(std::size_t bytes_processed, std::size_t total_bytes, double percentage)>;

// File metadata cache entry
struct FileMetadata {
    std::filesystem::file_status status;
    std::uintmax_t size;
    std::filesystem::file_time_type last_write_time;
    std::chrono::steady_clock::time_point cache_time;

    bool isValid(std::chrono::milliseconds max_age = std::chrono::milliseconds(5000)) const {
        auto now = std::chrono::steady_clock::now();
        return (now - cache_time) < max_age;
    }
};

/**
 * @brief Concept for valid path string types
 */
template <typename T>
concept PathString = std::convertible_to<T, std::string> ||
                     std::convertible_to<T, std::filesystem::path> ||
                     std::convertible_to<T, std::string_view>;

/**
 * @brief Enhanced context for managing async operations with cancellation and progress support
 */
class AsyncContext {
public:
    AsyncContext() = default;

    /**
     * @brief Checks if the context has been cancelled
     * @return True if cancelled, false otherwise
     */
    [[nodiscard]] bool is_cancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

    /**
     * @brief Cancels all operations using this context
     */
    void cancel() noexcept {
        cancelled_.store(true, std::memory_order_release);
        if (cancel_callback_) {
            cancel_callback_();
        }
    }

    /**
     * @brief Resets the cancellation state
     */
    void reset() noexcept {
        cancelled_.store(false, std::memory_order_release);
        progress_bytes_.store(0, std::memory_order_release);
        total_bytes_.store(0, std::memory_order_release);
    }

    /**
     * @brief Sets a callback to be called when cancellation occurs
     */
    void setCancelCallback(std::function<void()> callback) {
        cancel_callback_ = std::move(callback);
    }

    /**
     * @brief Updates progress information
     */
    void updateProgress(std::size_t bytes_processed, std::size_t total_bytes) {
        progress_bytes_.store(bytes_processed, std::memory_order_release);
        total_bytes_.store(total_bytes, std::memory_order_release);

        if (progress_callback_) {
            double percentage = total_bytes > 0 ?
                (static_cast<double>(bytes_processed) / total_bytes * 100.0) : 0.0;
            progress_callback_(bytes_processed, total_bytes, percentage);
        }
    }

    /**
     * @brief Sets a progress callback
     */
    void setProgressCallback(ProgressCallback callback) {
        progress_callback_ = std::move(callback);
    }

    /**
     * @brief Gets current progress
     */
    [[nodiscard]] std::pair<std::size_t, std::size_t> getProgress() const noexcept {
        return {progress_bytes_.load(std::memory_order_acquire),
                total_bytes_.load(std::memory_order_acquire)};
    }

private:
    std::atomic<bool> cancelled_{false};
    std::atomic<std::size_t> progress_bytes_{0};
    std::atomic<std::size_t> total_bytes_{0};
    std::function<void()> cancel_callback_;
    ProgressCallback progress_callback_;
};

/**
 * @brief Result type for async operations with enhanced error handling
 */
template <typename T>
struct AsyncResult {
    bool success{false};
    std::string error_message;
    T value{};

    /**
     * @brief Creates a successful result
     */
    static AsyncResult<T> success_result(T&& val) {
        AsyncResult<T> result;
        result.success = true;
        result.value = std::move(val);
        return result;
    }

    /**
     * @brief Creates a failed result
     */
    static AsyncResult<T> error_result(std::string_view error) {
        AsyncResult<T> result;
        result.success = false;
        result.error_message = error;
        return result;
    }
};

template <>
struct AsyncResult<void> {
    bool success{false};
    std::string error_message;

    /**
     * @brief Creates a successful result
     */
    static AsyncResult<void> success_result() {
        AsyncResult<void> result;
        result.success = true;
        return result;
    }

    /**
     * @brief Creates a failed result
     */
    static AsyncResult<void> error_result(std::string_view error) {
        AsyncResult<void> result;
        result.success = false;
        result.error_message = error;
        return result;
    }
};

template <typename T>
class [[nodiscard]] Task;

// Use the existing high-performance thread pool from atom::async namespace
using ThreadPool = atom::async::ThreadPool;

/**
 * @brief High-performance asynchronous file operations with enhanced features
 */
class AsyncFile {
public:
#ifdef ATOM_USE_ASIO
    /**
     * @brief Constructs an AsyncFile object with ASIO context
     * @param io_context The ASIO I/O context
     * @param config Configuration for async operations
     * @param context Optional async context for cancellation support
     */
    explicit AsyncFile(
        asio::io_context& io_context,
        const AsyncIOConfig& config = {},
        std::shared_ptr<AsyncContext> context = nullptr) noexcept;
#else
    /**
     * @brief Constructs an AsyncFile object with thread pool
     * @param config Configuration for async operations
     * @param context Optional async context for cancellation support
     */
    explicit AsyncFile(
        const AsyncIOConfig& config = {},
        std::shared_ptr<AsyncContext> context = nullptr) noexcept;
#endif

    /**
     * @brief Gets current statistics
     * @return Current I/O statistics
     */
    [[nodiscard]] const AsyncIOStats& getStats() const noexcept;

    /**
     * @brief Resets statistics
     */
    void resetStats() noexcept;

    /**
     * @brief Updates configuration
     * @param config New configuration
     */
    void updateConfig(const AsyncIOConfig& config) noexcept;

    /**
     * @brief Asynchronously reads file content with optimal performance
     * @param filename Path to the file to read
     * @param callback Callback function for the result
     */
    void asyncRead(PathString auto&& filename,
                   std::function<void(AsyncResult<std::string>)> callback);

    /**
     * @brief Asynchronously reads file content with progress reporting
     * @param filename Path to the file to read
     * @param progress_callback Progress callback function
     * @param completion_callback Completion callback function
     */
    void asyncReadWithProgress(PathString auto&& filename,
                              ProgressCallback progress_callback,
                              std::function<void(AsyncResult<std::string>)> completion_callback);

    /**
     * @brief Asynchronously writes content to a file
     * @param filename Path to the file to write
     * @param content Content to write as byte span
     * @param callback Callback function for the result
     */
    void asyncWrite(PathString auto&& filename, std::span<const char> content,
                    std::function<void(AsyncResult<void>)> callback);

    /**
     * @brief Asynchronously writes content with progress reporting
     * @param filename Path to the file to write
     * @param content Content to write as byte span
     * @param progress_callback Progress callback function
     * @param completion_callback Completion callback function
     */
    void asyncWriteWithProgress(PathString auto&& filename, std::span<const char> content,
                               ProgressCallback progress_callback,
                               std::function<void(AsyncResult<void>)> completion_callback);

    /**
     * @brief Asynchronously streams file content in chunks
     * @param filename Path to the file to read
     * @param chunk_callback Callback for each chunk
     * @param completion_callback Completion callback
     */
    void asyncStreamRead(PathString auto&& filename,
                        std::function<void(std::span<const char>)> chunk_callback,
                        std::function<void(AsyncResult<void>)> completion_callback);

    /**
     * @brief Asynchronously deletes a file
     * @param filename Path to the file to delete
     * @param callback Callback function for the result
     */
    void asyncDelete(PathString auto&& filename,
                     std::function<void(AsyncResult<void>)> callback);

    /**
     * @brief Asynchronously copies a file with optimized buffering
     * @param src Source file path
     * @param dest Destination file path
     * @param callback Callback function for the result
     */
    void asyncCopy(PathString auto&& src, PathString auto&& dest,
                   std::function<void(AsyncResult<void>)> callback);

    /**
     * @brief Asynchronously moves/renames a file
     * @param src Source file path
     * @param dest Destination file path
     * @param callback Callback function for the result
     */
    void asyncMove(PathString auto&& src, PathString auto&& dest,
                   std::function<void(AsyncResult<void>)> callback);

    /**
     * @brief Asynchronously reads file with timeout support
     * @param filename Path to the file to read
     * @param timeout Maximum time to wait for completion
     * @param callback Callback function for the result
     */
    void asyncReadWithTimeout(
        PathString auto&& filename, std::chrono::milliseconds timeout,
        std::function<void(AsyncResult<std::string>)> callback);

    /**
     * @brief Efficiently reads multiple files in parallel
     * @param files List of file paths to read
     * @param callback Callback function for the results
     */
    void asyncBatchRead(
        std::span<const std::string> files,
        std::function<void(AsyncResult<std::vector<std::string>>)> callback);

    /**
     * @brief Asynchronously retrieves file status information
     * @param filename Path to the file
     * @param callback Callback function for the file status
     */
    void asyncStat(
        PathString auto&& filename,
        std::function<void(AsyncResult<std::filesystem::file_status>)>
            callback);

    /**
     * @brief Asynchronously changes file permissions
     * @param filename Path to the file
     * @param perms New permissions to set
     * @param callback Callback function for the result
     */
    void asyncChangePermissions(
        PathString auto&& filename, std::filesystem::perms perms,
        std::function<void(AsyncResult<void>)> callback);

    /**
     * @brief Asynchronously checks if a file exists
     * @param filename Path to the file
     * @param callback Callback function for the existence check result
     */
    void asyncExists(PathString auto&& filename,
                     std::function<void(AsyncResult<bool>)> callback);

    /**
     * @brief Coroutine-based file reading with enhanced performance
     * @param filename Path to the file to read
     * @return Task that completes with file content
     */
    [[nodiscard]] Task<AsyncResult<std::string>> readFile(
        PathString auto&& filename);

    /**
     * @brief Coroutine-based file writing with enhanced performance
     * @param filename Path to the file to write
     * @param content Content to write as byte span
     * @return Task that completes when write operation finishes
     */
    [[nodiscard]] Task<AsyncResult<void>> writeFile(
        PathString auto&& filename, std::span<const char> content);

    /**
     * @brief Asynchronously creates a directory (consolidated functionality)
     * @param path Path of the directory to create
     * @param callback Callback function for the result
     */
    void asyncCreateDirectory(PathString auto&& path,
                              std::function<void(AsyncResult<void>)> callback);

    /**
     * @brief Asynchronously removes a directory (consolidated functionality)
     * @param path Path of the directory to remove
     * @param callback Callback function for the result
     */
    void asyncRemoveDirectory(PathString auto&& path,
                              std::function<void(AsyncResult<void>)> callback);

    /**
     * @brief Asynchronously lists directory contents (consolidated
     * functionality)
     * @param path Path of the directory to list
     * @param callback Callback function for the directory contents
     */
    void asyncListDirectory(
        PathString auto&& path,
        std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>
            callback);

    /**
     * @brief Coroutine-based directory listing
     * @param path Path of the directory to list
     * @return Task that completes with directory contents
     */
    [[nodiscard]] Task<AsyncResult<std::vector<std::filesystem::path>>>
    listDirectory(PathString auto&& path);

private:
#ifdef ATOM_USE_ASIO
    asio::io_context& io_context_;
    std::shared_ptr<asio::steady_timer> timer_;
#else
    std::shared_ptr<ThreadPool> thread_pool_;
#endif

    AsyncIOConfig config_;
    mutable AsyncIOStats stats_;
    std::shared_ptr<AsyncContext> context_;
    std::shared_ptr<spdlog::logger> logger_;

    // File metadata cache
    mutable std::unordered_map<std::string, FileMetadata> metadata_cache_;
    mutable std::mutex cache_mutex_;

    // Buffer pool for efficient memory management
    std::vector<std::vector<char>> buffer_pool_;
    std::mutex buffer_pool_mutex_;

    /**
     * @brief Validates a path for security and format
     * @param path Path to validate
     * @return True if valid, false otherwise
     */
    static bool validatePath(std::string_view path) noexcept;

    /**
     * @brief Converts path-like types to string efficiently
     * @param path Path to convert
     * @return String representation of the path
     */
    template <PathString T>
    static std::string toString(T&& path);

    /**
     * @brief Gets or creates file metadata with caching
     * @param path File path
     * @return File metadata or nullopt if error
     */
    std::optional<FileMetadata> getFileMetadata(const std::string& path) const;

    /**
     * @brief Gets a buffer from the pool or creates a new one
     * @param size Required buffer size
     * @return Buffer vector
     */
    std::vector<char> getBuffer(std::size_t size);

    /**
     * @brief Returns a buffer to the pool
     * @param buffer Buffer to return
     */
    void returnBuffer(std::vector<char>&& buffer);

    /**
     * @brief Cleans up expired cache entries
     */
    void cleanupCache() const;

#ifndef ATOM_USE_ASIO
    template <typename F>
    void scheduleTimeout(std::chrono::milliseconds timeout, F&& callback);
#endif

    /**
     * @brief Generic async operation executor with context support
     */
    template <typename F>
    void executeAsync(F&& operation);

    /**
     * @brief Executes file operation with proper error handling and statistics
     */
    template <typename F>
    void executeFileOperation(F&& operation, const std::string& operation_name);
};

/**
 * @brief Legacy AsyncDirectory interface for backward compatibility
 * @deprecated Use AsyncFile methods instead for unified interface
 */
class [[deprecated(
    "Use AsyncFile for unified file/directory operations")]] AsyncDirectory {
public:
#ifdef ATOM_USE_ASIO
    explicit AsyncDirectory(asio::io_context& io_context) noexcept;
#else
    explicit AsyncDirectory() noexcept;
#endif

    void asyncCreate(PathString auto&& path,
                     std::function<void(AsyncResult<void>)> callback);
    void asyncRemove(PathString auto&& path,
                     std::function<void(AsyncResult<void>)> callback);
    void asyncListContents(
        PathString auto&& path,
        std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>
            callback);
    void asyncExists(PathString auto&& path,
                     std::function<void(AsyncResult<bool>)> callback);
    [[nodiscard]] Task<AsyncResult<std::vector<std::filesystem::path>>>
    listContents(PathString auto&& path);

private:
    std::unique_ptr<AsyncFile> file_impl_;
};

/**
 * @brief High-performance coroutine Task implementation with cancellation
 * support
 */
template <typename T>
class [[nodiscard]] Task {
public:
    struct promise_type {
        std::promise<T> promise;
        std::weak_ptr<AsyncContext> context;

        Task get_return_object() noexcept {
            return Task(promise.get_future(), context.lock());
        }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_value(T value) noexcept {
            if (auto ctx = context.lock(); !ctx || !ctx->is_cancelled()) {
                promise.set_value(std::move(value));
            }
        }

        void unhandled_exception() noexcept {
            try {
                std::rethrow_exception(std::current_exception());
            } catch (const std::exception& e) {
                T failed_result;
                if constexpr (std::is_same_v<T, AsyncResult<void>>) {
                    failed_result = AsyncResult<void>::error_result(e.what());
                } else {
                    failed_result =
                        AsyncResult<typename T::value_type>::error_result(
                            e.what());
                }
                promise.set_value(std::move(failed_result));
            }
        }

        void set_context(std::shared_ptr<AsyncContext> ctx) { context = ctx; }
    };

    explicit Task(std::future<T> future,
                  std::shared_ptr<AsyncContext> ctx = nullptr) noexcept
        : future_(std::move(future)), context_(std::move(ctx)) {}

    /**
     * @brief Gets the result, blocking if necessary
     * @return The task result
     */
    T get() { return future_.get(); }

    /**
     * @brief Checks if the task is ready without blocking
     * @return True if ready, false otherwise
     */
    [[nodiscard]] bool is_ready() const noexcept {
        return future_.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready;
    }

    /**
     * @brief Waits for the task to complete with timeout
     * @param timeout Maximum time to wait
     * @return Future status
     */
    template <typename Rep, typename Period>
    [[nodiscard]] std::future_status wait_for(
        const std::chrono::duration<Rep, Period>& timeout) const {
        return future_.wait_for(timeout);
    }

    /**
     * @brief Cancels the task if context is available
     */
    void cancel() {
        if (context_) {
            context_->cancel();
        }
    }

    /**
     * @brief Checks if the task is cancelled
     * @return True if cancelled, false otherwise
     */
    [[nodiscard]] bool is_cancelled() const noexcept {
        return context_ && context_->is_cancelled();
    }

private:
    std::future<T> future_;
    std::shared_ptr<AsyncContext> context_;
};

// Template implementations for new enhanced methods

template <PathString T>
void AsyncFile::asyncReadWithProgress(T&& filename,
                                     ProgressCallback progress_callback,
                                     std::function<void(AsyncResult<std::string>)> completion_callback) {
    std::string path = toString(std::forward<T>(filename));

    if (!validatePath(path)) {
        completion_callback(AsyncResult<std::string>::error_result("Invalid file path"));
        return;
    }

    executeFileOperation([this, path, progress_callback, completion_callback]() {
        try {
            auto metadata = getFileMetadata(path);
            if (!metadata) {
                completion_callback(AsyncResult<std::string>::error_result("Cannot access file metadata"));
                return;
            }

            std::ifstream file(path, std::ios::binary);
            if (!file) {
                completion_callback(AsyncResult<std::string>::error_result("Cannot open file for reading"));
                return;
            }

            std::string content;
            content.reserve(metadata->size);

            auto buffer = getBuffer(config_.buffer_size);
            std::size_t total_read = 0;

            while (file && !file.eof() && (!context_ || !context_->is_cancelled())) {
                file.read(buffer.data(), buffer.size());
                auto bytes_read = file.gcount();

                if (bytes_read > 0) {
                    content.append(buffer.data(), bytes_read);
                    total_read += bytes_read;

                    if (progress_callback && metadata->size > 0) {
                        double percentage = static_cast<double>(total_read) / metadata->size * 100.0;
                        progress_callback(total_read, metadata->size, percentage);
                    }

                    if (context_) {
                        context_->updateProgress(total_read, metadata->size);
                    }
                }
            }

            returnBuffer(std::move(buffer));
            stats_.files_read++;
            stats_.bytes_read += total_read;

            if (context_ && context_->is_cancelled()) {
                completion_callback(AsyncResult<std::string>::error_result("Operation cancelled"));
            } else {
                completion_callback(AsyncResult<std::string>::success_result(std::move(content)));
            }
        } catch (const std::exception& e) {
            completion_callback(AsyncResult<std::string>::error_result(e.what()));
        }
    }, "asyncReadWithProgress");
}

template <PathString T>
void AsyncFile::asyncWriteWithProgress(T&& filename, std::span<const char> content,
                                      ProgressCallback progress_callback,
                                      std::function<void(AsyncResult<void>)> completion_callback) {
    std::string path = toString(std::forward<T>(filename));

    if (!validatePath(path)) {
        completion_callback(AsyncResult<void>::error_result("Invalid file path"));
        return;
    }

    executeFileOperation([this, path, content, progress_callback, completion_callback]() {
        try {
            std::ofstream file(path, std::ios::binary);
            if (!file) {
                completion_callback(AsyncResult<void>::error_result("Cannot open file for writing"));
                return;
            }

            std::size_t total_written = 0;
            std::size_t total_size = content.size();
            std::size_t chunk_size = std::min(config_.buffer_size, total_size);

            for (std::size_t offset = 0; offset < total_size && (!context_ || !context_->is_cancelled()); offset += chunk_size) {
                std::size_t bytes_to_write = std::min(chunk_size, total_size - offset);

                file.write(content.data() + offset, bytes_to_write);
                if (!file) {
                    completion_callback(AsyncResult<void>::error_result("Write operation failed"));
                    return;
                }

                total_written += bytes_to_write;

                if (progress_callback) {
                    double percentage = static_cast<double>(total_written) / total_size * 100.0;
                    progress_callback(total_written, total_size, percentage);
                }

                if (context_) {
                    context_->updateProgress(total_written, total_size);
                }
            }

            stats_.files_written++;
            stats_.bytes_written += total_written;

            if (context_ && context_->is_cancelled()) {
                completion_callback(AsyncResult<void>::error_result("Operation cancelled"));
            } else {
                completion_callback(AsyncResult<void>::success_result());
            }
        } catch (const std::exception& e) {
            completion_callback(AsyncResult<void>::error_result(e.what()));
        }
    }, "asyncWriteWithProgress");
}

template <PathString T>
void AsyncFile::asyncStreamRead(T&& filename,
                               std::function<void(std::span<const char>)> chunk_callback,
                               std::function<void(AsyncResult<void>)> completion_callback) {
    std::string path = toString(std::forward<T>(filename));

    if (!validatePath(path)) {
        completion_callback(AsyncResult<void>::error_result("Invalid file path"));
        return;
    }

    executeFileOperation([this, path, chunk_callback, completion_callback]() {
        try {
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                completion_callback(AsyncResult<void>::error_result("Cannot open file for reading"));
                return;
            }

            auto buffer = getBuffer(config_.buffer_size);
            std::size_t total_read = 0;

            while (file && !file.eof() && (!context_ || !context_->is_cancelled())) {
                file.read(buffer.data(), buffer.size());
                auto bytes_read = file.gcount();

                if (bytes_read > 0) {
                    chunk_callback(std::span<const char>(buffer.data(), bytes_read));
                    total_read += bytes_read;
                }
            }

            returnBuffer(std::move(buffer));
            stats_.files_read++;
            stats_.bytes_read += total_read;

            if (context_ && context_->is_cancelled()) {
                completion_callback(AsyncResult<void>::error_result("Operation cancelled"));
            } else {
                completion_callback(AsyncResult<void>::success_result());
            }
        } catch (const std::exception& e) {
            completion_callback(AsyncResult<void>::error_result(e.what()));
        }
    }, "asyncStreamRead");
}

}  // namespace atom::async::io

#endif  // ATOM_IO_ASYNC_IO_HPP

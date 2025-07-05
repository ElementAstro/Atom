#ifndef ATOM_IO_ASYNC_IO_HPP
#define ATOM_IO_ASYNC_IO_HPP

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

#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

#include <spdlog/spdlog.h>
#include "atom/async/pool.hpp"

namespace atom::async::io {

/**
 * @brief Concept for valid path string types
 */
template <typename T>
concept PathString = std::convertible_to<T, std::string> ||
                     std::convertible_to<T, std::filesystem::path> ||
                     std::convertible_to<T, std::string_view>;

/**
 * @brief Context for managing async operations with cancellation support
 */
class AsyncContext {
public:
    AsyncContext() = default;

    /**
     * @brief Checks if the context has been cancelled
     * @return True if cancelled, false otherwise
     */
    [[nodiscard]] bool is_cancelled() const noexcept {
        return cancelled_.load();
    }

    /**
     * @brief Cancels all operations using this context
     */
    void cancel() noexcept { cancelled_.store(true); }

    /**
     * @brief Resets the cancellation state
     */
    void reset() noexcept { cancelled_.store(false); }

private:
    std::atomic<bool> cancelled_{false};
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
 * @brief High-performance asynchronous file operations with context support
 */
class AsyncFile {
public:
#ifdef ATOM_USE_ASIO
    /**
     * @brief Constructs an AsyncFile object with ASIO context
     * @param io_context The ASIO I/O context
     * @param context Optional async context for cancellation support
     */
    explicit AsyncFile(
        asio::io_context& io_context,
        std::shared_ptr<AsyncContext> context = nullptr) noexcept;
#else
    /**
     * @brief Constructs an AsyncFile object with thread pool
     * @param context Optional async context for cancellation support
     */
    explicit AsyncFile(
        std::shared_ptr<AsyncContext> context = nullptr) noexcept;
#endif

    /**
     * @brief Asynchronously reads file content with optimal performance
     * @param filename Path to the file to read
     * @param callback Callback function for the result
     */
    void asyncRead(PathString auto&& filename,
                   std::function<void(AsyncResult<std::string>)> callback);

    /**
     * @brief Asynchronously writes content to a file
     * @param filename Path to the file to write
     * @param content Content to write as byte span
     * @param callback Callback function for the result
     */
    void asyncWrite(PathString auto&& filename, std::span<const char> content,
                    std::function<void(AsyncResult<void>)> callback);

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

    std::shared_ptr<AsyncContext> context_;
    std::shared_ptr<spdlog::logger> logger_;

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

#ifndef ATOM_USE_ASIO
    template <typename F>
    void scheduleTimeout(std::chrono::milliseconds timeout, F&& callback);
#endif

    /**
     * @brief Generic async operation executor with context support
     */
    template <typename F>
    void executeAsync(F&& operation);
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

}  // namespace atom::async::io

#endif  // ATOM_IO_ASYNC_IO_HPP

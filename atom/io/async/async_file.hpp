#ifndef ATOM_IO_ASYNC_ASYNC_FILE_HPP
#define ATOM_IO_ASYNC_ASYNC_FILE_HPP

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "async_types.hpp"

namespace atom::io::async {

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
    [[nodiscard]] atom::async::Task<AsyncResult<std::string>> readFile(
        PathString auto&& filename);

    /**
     * @brief Coroutine-based file writing with enhanced performance
     * @param filename Path to the file to write
     * @param content Content to write as byte span
     * @return Task that completes when write operation finishes
     */
    [[nodiscard]] atom::async::Task<AsyncResult<void>> writeFile(
        PathString auto&& filename, std::span<const char> content);

    /**
     * @brief Coroutine-based file deletion
     * @param filename Path to the file to delete
     * @return Task that completes when deletion finishes
     */
    [[nodiscard]] atom::async::Task<AsyncResult<void>> deleteFile(
        PathString auto&& filename);

    /**
     * @brief Coroutine-based file status retrieval
     * @param filename Path to the file
     * @return Task that completes with file status
     */
    [[nodiscard]] atom::async::Task<AsyncResult<std::filesystem::file_status>>
    getFileStatus(PathString auto&& filename);

    /**
     * @brief Coroutine-based file existence check
     * @param filename Path to the file
     * @return Task that completes with existence result
     */
    [[nodiscard]] atom::async::Task<AsyncResult<bool>> fileExists(
        PathString auto&& filename);

    /**
     * @brief Coroutine-based permission change
     * @param filename Path to the file
     * @param perms New permissions to set
     * @return Task that completes when permission change finishes
     */
    [[nodiscard]] atom::async::Task<AsyncResult<void>> changePermissions(
        PathString auto&& filename, std::filesystem::perms perms);

    /**
     * @brief Validates a path for security and format
     * @param path Path to validate
     * @return True if valid, false otherwise
     */
    static bool validatePath(std::string_view path) noexcept;

    /**
     * @brief Validates file permissions for read/write operations
     * @param path Path to validate
     * @param write_access Whether write access is required
     * @return True if permissions are valid, false otherwise
     */
    static bool validatePermissions(std::string_view path,
                                    bool write_access = false) noexcept;

    /**
     * @brief Sanitizes a filename by removing or replacing invalid characters
     * @param filename Filename to sanitize
     * @return Sanitized filename
     */
    static std::string sanitizeFilename(std::string_view filename) noexcept;

    /**
     * @brief Converts path-like types to string efficiently
     * @param path Path to convert
     * @return String representation of the path
     */
    template <PathString T>
    static std::string toString(T&& path);

    /**
     * @brief Generic async operation executor with context support
     */
    template <typename F>
    void executeAsync(F&& operation);

#ifndef ATOM_USE_ASIO
    template <typename F>
    void scheduleTimeout(std::chrono::milliseconds timeout, F&& callback);
#endif

private:
#ifdef ATOM_USE_ASIO
    asio::io_context& io_context_;
    std::shared_ptr<asio::steady_timer> timer_;
#else
    std::shared_ptr<ThreadPool> thread_pool_;
#endif

    std::shared_ptr<AsyncContext> context_;
    std::shared_ptr<spdlog::logger> logger_;
};

// Template method implementations for AsyncFile
template <PathString T>
std::string AsyncFile::toString(T&& path) {
    if constexpr (std::convertible_to<T, std::string_view>) {
        return std::string(std::forward<T>(path));
    } else if constexpr (std::convertible_to<T, std::filesystem::path>) {
        return std::filesystem::path(std::forward<T>(path)).string();
    } else {
        return std::string(std::forward<T>(path));
    }
}

template <typename F>
void AsyncFile::executeAsync(F&& operation) {
    if (context_ && context_->is_cancelled()) {
        return;
    }

#ifdef ATOM_USE_ASIO
    asio::post(io_context_, std::forward<F>(operation));
#else
    if (thread_pool_) {
        thread_pool_->submit(std::forward<F>(operation));
    } else {
        // Fallback to immediate execution
        operation();
    }
#endif
}

template <PathString T>
void AsyncFile::asyncRead(
    T&& filename, std::function<void(AsyncResult<std::string>)> callback) {
    executeAsync([filename = toString(std::forward<T>(filename)),
                  callback = std::move(callback)]() {
        try {
            std::ifstream file(filename, std::ios::binary);
            if (!file) {
                callback(AsyncResult<std::string>::error_result(
                    "Failed to open file: " + filename));
                return;
            }

            file.seekg(0, std::ios::end);
            auto size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::string content(size, '\0');
            if (!file.read(content.data(), size)) {
                callback(AsyncResult<std::string>::error_result(
                    "Failed to read file: " + filename));
                return;
            }

            callback(
                AsyncResult<std::string>::success_result(std::move(content)));
        } catch (const std::exception& e) {
            callback(AsyncResult<std::string>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncWrite(T&& filename, std::span<const char> content,
                           std::function<void(AsyncResult<void>)> callback) {
    executeAsync([filename = toString(std::forward<T>(filename)),
                  content = std::string(content.begin(), content.end()),
                  callback = std::move(callback)]() {
        try {
            std::ofstream file(filename, std::ios::binary);
            if (!file) {
                callback(AsyncResult<void>::error_result(
                    "Failed to open file for writing: " + filename));
                return;
            }

            if (!file.write(content.data(), content.size())) {
                callback(AsyncResult<void>::error_result(
                    "Failed to write to file: " + filename));
                return;
            }

            callback(AsyncResult<void>::success_result());
        } catch (const std::exception& e) {
            callback(AsyncResult<void>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncDelete(T&& filename,
                            std::function<void(AsyncResult<void>)> callback) {
    executeAsync([filename = toString(std::forward<T>(filename)),
                  callback = std::move(callback)]() {
        try {
            if (std::filesystem::remove(filename)) {
                callback(AsyncResult<void>::success_result());
            } else {
                callback(AsyncResult<void>::error_result(
                    "Failed to delete file: " + filename));
            }
        } catch (const std::exception& e) {
            callback(AsyncResult<void>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncStat(
    T&& filename,
    std::function<void(AsyncResult<std::filesystem::file_status>)> callback) {
    executeAsync([filename = toString(std::forward<T>(filename)),
                  callback = std::move(callback)]() {
        try {
            std::error_code ec;
            auto status = std::filesystem::status(filename, ec);
            if (ec) {
                callback(
                    AsyncResult<std::filesystem::file_status>::error_result(
                        "Failed to get file status: " + filename + " - " +
                        ec.message()));
                return;
            }
            callback(AsyncResult<std::filesystem::file_status>::success_result(
                std::move(status)));
        } catch (const std::exception& e) {
            callback(AsyncResult<std::filesystem::file_status>::error_result(
                e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncChangePermissions(
    T&& filename, std::filesystem::perms perms,
    std::function<void(AsyncResult<void>)> callback) {
    executeAsync([filename = toString(std::forward<T>(filename)), perms,
                  callback = std::move(callback)]() {
        try {
            std::error_code ec;
            std::filesystem::permissions(filename, perms, ec);
            if (ec) {
                callback(AsyncResult<void>::error_result(
                    "Failed to change permissions: " + filename + " - " +
                    ec.message()));
                return;
            }
            callback(AsyncResult<void>::success_result());
        } catch (const std::exception& e) {
            callback(AsyncResult<void>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncExists(T&& filename,
                            std::function<void(AsyncResult<bool>)> callback) {
    executeAsync([filename = toString(std::forward<T>(filename)),
                  callback = std::move(callback)]() {
        try {
            bool exists = std::filesystem::exists(filename);
            callback(AsyncResult<bool>::success_result(std::move(exists)));
        } catch (const std::exception& e) {
            callback(AsyncResult<bool>::error_result(e.what()));
        }
    });
}

template <PathString T, PathString U>
void AsyncFile::asyncCopy(T&& src, U&& dest,
                          std::function<void(AsyncResult<void>)> callback) {
    executeAsync([src = toString(std::forward<T>(src)),
                  dest = toString(std::forward<U>(dest)),
                  callback = std::move(callback)]() {
        try {
            std::error_code ec;
            std::filesystem::copy_file(
                src, dest, std::filesystem::copy_options::overwrite_existing,
                ec);

            if (ec) {
                callback(AsyncResult<void>::error_result(
                    "Failed to copy file from " + src + " to " + dest + ": " +
                    ec.message()));
                return;
            }

            callback(AsyncResult<void>::success_result());
        } catch (const std::exception& e) {
            callback(AsyncResult<void>::error_result(e.what()));
        }
    });
}

template <PathString T, PathString U>
void AsyncFile::asyncMove(T&& src, U&& dest,
                          std::function<void(AsyncResult<void>)> callback) {
    executeAsync([src = toString(std::forward<T>(src)),
                  dest = toString(std::forward<U>(dest)),
                  callback = std::move(callback)]() {
        try {
            std::error_code ec;
            std::filesystem::rename(src, dest, ec);

            if (ec) {
                callback(AsyncResult<void>::error_result(
                    "Failed to move file from " + src + " to " + dest + ": " +
                    ec.message()));
                return;
            }

            callback(AsyncResult<void>::success_result());
        } catch (const std::exception& e) {
            callback(AsyncResult<void>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncReadWithTimeout(
    T&& filename, std::chrono::milliseconds timeout,
    std::function<void(AsyncResult<std::string>)> callback) {
    auto filename_str = toString(std::forward<T>(filename));

    // Launch the read operation asynchronously
    auto read_future = std::async(
        std::launch::async, [filename_str]() -> AsyncResult<std::string> {
            try {
                std::ifstream file(filename_str, std::ios::binary);
                if (!file) {
                    return AsyncResult<std::string>::error_result(
                        "Failed to open file: " + filename_str);
                }

                file.seekg(0, std::ios::end);
                auto size = file.tellg();
                file.seekg(0, std::ios::beg);

                std::string content(size, '\0');
                if (!file.read(content.data(), size)) {
                    return AsyncResult<std::string>::error_result(
                        "Failed to read file: " + filename_str);
                }

                return AsyncResult<std::string>::success_result(
                    std::move(content));
            } catch (const std::exception& e) {
                return AsyncResult<std::string>::error_result(e.what());
            }
        });

    // Wait for completion with timeout
    executeAsync([read_future = std::move(read_future), timeout,
                  callback = std::move(callback)]() mutable {
        if (read_future.wait_for(timeout) == std::future_status::timeout) {
            callback(AsyncResult<std::string>::error_result(
                "Read operation timed out"));
        } else {
            callback(read_future.get());
        }
    });
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<std::string>> AsyncFile::readFile(
    T&& filename) {
    std::promise<AsyncResult<std::string>> promise;
    auto future = promise.get_future();

    asyncRead(std::forward<T>(filename),
              [&promise](AsyncResult<std::string> result) {
                  promise.set_value(std::move(result));
              });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<void>> AsyncFile::writeFile(
    T&& filename, std::span<const char> content) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    asyncWrite(std::forward<T>(filename), content,
               [&promise](AsyncResult<void> result) {
                   promise.set_value(std::move(result));
               });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<void>> AsyncFile::deleteFile(
    T&& filename) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    asyncDelete(std::forward<T>(filename),
                [&promise](AsyncResult<void> result) {
                    promise.set_value(std::move(result));
                });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<std::filesystem::file_status>>
AsyncFile::getFileStatus(T&& filename) {
    std::promise<AsyncResult<std::filesystem::file_status>> promise;
    auto future = promise.get_future();

    asyncStat(std::forward<T>(filename),
              [&promise](AsyncResult<std::filesystem::file_status> result) {
                  promise.set_value(std::move(result));
              });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<bool>> AsyncFile::fileExists(
    T&& filename) {
    std::promise<AsyncResult<bool>> promise;
    auto future = promise.get_future();

    asyncExists(std::forward<T>(filename),
                [&promise](AsyncResult<bool> result) {
                    promise.set_value(std::move(result));
                });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<void>> AsyncFile::changePermissions(
    T&& filename, std::filesystem::perms perms) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    asyncChangePermissions(std::forward<T>(filename), perms,
                           [&promise](AsyncResult<void> result) {
                               promise.set_value(std::move(result));
                           });

    co_return future.get();
}

}  // namespace atom::io::async

#endif  // ATOM_IO_ASYNC_ASYNC_FILE_HPP

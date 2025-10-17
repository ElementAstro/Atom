#ifndef ATOM_IO_ASYNC_ASYNC_IO_HPP
#define ATOM_IO_ASYNC_ASYNC_IO_HPP

#include <chrono>
#include <concepts>
#include <coroutine>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "atom/async/core/async.hpp"

#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

// SIMD support detection
#ifdef __SSE4_2__
#include <immintrin.h>
#define ATOM_HAS_SSE42 1
#endif

#ifdef __AVX2__
#include <immintrin.h>
#define ATOM_HAS_AVX2 1
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
 * @brief Concept for types that can be used as file content
 */
template <typename T>
concept FileContent = std::ranges::contiguous_range<T> &&
                     (std::same_as<std::ranges::range_value_t<T>, char> ||
                      std::same_as<std::ranges::range_value_t<T>, unsigned char> ||
                      std::same_as<std::ranges::range_value_t<T>, std::byte>);

/**
 * @brief Concept for callback functions that can handle AsyncResult
 * Note: This concept is defined after AsyncResult class
 */

/**
 * @brief Concept for types that can be used as file permissions
 */
template <typename T>
concept FilePermissions = std::same_as<T, std::filesystem::perms> ||
                         std::convertible_to<T, std::filesystem::perms>;

/**
 * @brief Concept for types that represent file sizes
 */
template <typename T>
concept FileSizeType = std::integral<T> && std::unsigned_integral<T>;

/**
 * @brief Concept for types that can be used as buffer data
 */
template <typename T>
concept BufferData = std::ranges::contiguous_range<T> &&
                    std::is_trivially_copyable_v<std::ranges::range_value_t<T>>;

/**
 * @brief Concept for types that can be used as compression options
 */
template <typename T>
concept CompressionOptionsType = requires(T t) {
    { t.compression_level } -> std::convertible_to<int>;
    { t.enable_checksum } -> std::convertible_to<bool>;
};

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

/**
 * @brief Concept for callback functions that can handle AsyncResult
 */
template <typename F, typename T>
concept AsyncResultCallback = std::invocable<F, AsyncResult<T>>;

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
    [[nodiscard]] atom::async::Task<AsyncResult<std::vector<std::filesystem::path>>>
    listDirectory(PathString auto&& path);

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
    [[nodiscard]] atom::async::Task<AsyncResult<std::filesystem::file_status>> getFileStatus(
        PathString auto&& filename);

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
     * @brief Coroutine-based directory creation
     * @param path Path of the directory to create
     * @return Task that completes when creation finishes
     */
    [[nodiscard]] atom::async::Task<AsyncResult<void>> createDirectory(
        PathString auto&& path);

    /**
     * @brief Coroutine-based directory removal
     * @param path Path of the directory to remove
     * @return Task that completes when removal finishes
     */
    [[nodiscard]] atom::async::Task<AsyncResult<void>> removeDirectory(
        PathString auto&& path);

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
     * @brief Validates file permissions for read/write operations
     * @param path Path to validate
     * @param write_access Whether write access is required
     * @return True if permissions are valid, false otherwise
     */
    static bool validatePermissions(std::string_view path, bool write_access = false) noexcept;

    /**
     * @brief Sanitizes a filename by removing or replacing invalid characters
     * @param filename Filename to sanitize
     * @return Sanitized filename
     */
    static std::string sanitizeFilename(std::string_view filename) noexcept;

    /**
     * @brief SIMD-optimized buffer operations for high-performance I/O
     */
    static bool simdBufferCompare(std::span<const char> buffer1, std::span<const char> buffer2) noexcept;
    static size_t simdFindByte(std::span<const char> buffer, char target) noexcept;
    static void simdMemorySet(std::span<char> buffer, char value) noexcept;

    /**
     * @brief Performance optimization functions
     */
    /**
     * @brief Asynchronously writes multiple files in parallel for optimal performance
     * @param file_data_pairs Pairs of filename and data to write
     * @param callback Callback function for the batch operation result
     */
    void asyncBatchWrite(std::span<const std::pair<std::string, std::string>> file_data_pairs,
                        std::function<void(AsyncResult<void>)> callback);

    /**
     * @brief Asynchronously deletes multiple files in parallel
     * @param files List of file paths to delete
     * @param callback Callback function for the batch operation result
     */
    void asyncBatchDelete(std::span<const std::string> files,
                         std::function<void(AsyncResult<void>)> callback);
    /**
     * @brief Asynchronously reads a file in chunks for memory-efficient processing
     * @param filename Path to the file to read
     * @param chunk_size Size of each chunk to read
     * @param chunk_callback Callback for each chunk read
     * @param completion_callback Callback when reading is complete
     */
    template <PathString T>
    void asyncStreamRead(T&& filename, size_t chunk_size,
                        std::function<void(AsyncResult<std::string>)> chunk_callback,
                        std::function<void(AsyncResult<void>)> completion_callback);

    /**
     * @brief Asynchronously writes data to a file in chunks for memory efficiency
     * @param filename Path to the file to write
     * @param data Data to write
     * @param chunk_size Size of each chunk to write
     * @param callback Callback when writing is complete
     */
    template <PathString T>
    void asyncStreamWrite(T&& filename, std::span<const char> data,
                         size_t chunk_size, std::function<void(AsyncResult<void>)> callback);

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
class [[deprecated("Use AsyncFile for unified file/directory operations")]]
AsyncDirectory {
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
void AsyncFile::asyncRead(T&& filename,
                         std::function<void(AsyncResult<std::string>)> callback) {
    executeAsync([filename = toString(std::forward<T>(filename)), callback = std::move(callback)]() {
        try {
            std::ifstream file(filename, std::ios::binary);
            if (!file) {
                callback(AsyncResult<std::string>::error_result("Failed to open file: " + filename));
                return;
            }

            file.seekg(0, std::ios::end);
            auto size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::string content(size, '\0');
            if (!file.read(content.data(), size)) {
                callback(AsyncResult<std::string>::error_result("Failed to read file: " + filename));
                return;
            }

            callback(AsyncResult<std::string>::success_result(std::move(content)));
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
                callback(AsyncResult<void>::error_result("Failed to open file for writing: " + filename));
                return;
            }

            if (!file.write(content.data(), content.size())) {
                callback(AsyncResult<void>::error_result("Failed to write to file: " + filename));
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
    executeAsync([filename = toString(std::forward<T>(filename)), callback = std::move(callback)]() {
        try {
            if (std::filesystem::remove(filename)) {
                callback(AsyncResult<void>::success_result());
            } else {
                callback(AsyncResult<void>::error_result("Failed to delete file: " + filename));
            }
        } catch (const std::exception& e) {
            callback(AsyncResult<void>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncStat(T&& filename,
                         std::function<void(AsyncResult<std::filesystem::file_status>)> callback) {
    executeAsync([filename = toString(std::forward<T>(filename)), callback = std::move(callback)]() {
        try {
            std::error_code ec;
            auto status = std::filesystem::status(filename, ec);
            if (ec) {
                callback(AsyncResult<std::filesystem::file_status>::error_result(
                    "Failed to get file status: " + filename + " - " + ec.message()));
                return;
            }
            callback(AsyncResult<std::filesystem::file_status>::success_result(std::move(status)));
        } catch (const std::exception& e) {
            callback(AsyncResult<std::filesystem::file_status>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncChangePermissions(T&& filename, std::filesystem::perms perms,
                                      std::function<void(AsyncResult<void>)> callback) {
    executeAsync([filename = toString(std::forward<T>(filename)), perms, callback = std::move(callback)]() {
        try {
            std::error_code ec;
            std::filesystem::permissions(filename, perms, ec);
            if (ec) {
                callback(AsyncResult<void>::error_result(
                    "Failed to change permissions: " + filename + " - " + ec.message()));
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
    executeAsync([filename = toString(std::forward<T>(filename)), callback = std::move(callback)]() {
        try {
            bool exists = std::filesystem::exists(filename);
            callback(AsyncResult<bool>::success_result(std::move(exists)));
        } catch (const std::exception& e) {
            callback(AsyncResult<bool>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncCreateDirectory(T&& path,
                                    std::function<void(AsyncResult<void>)> callback) {
    executeAsync([path = toString(std::forward<T>(path)), callback = std::move(callback)]() {
        try {
            std::error_code ec;
            std::filesystem::create_directories(path, ec);
            if (ec) {
                callback(AsyncResult<void>::error_result(
                    "Failed to create directory: " + path + " - " + ec.message()));
                return;
            }
            callback(AsyncResult<void>::success_result());
        } catch (const std::exception& e) {
            callback(AsyncResult<void>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncRemoveDirectory(T&& path,
                                    std::function<void(AsyncResult<void>)> callback) {
    executeAsync([path = toString(std::forward<T>(path)), callback = std::move(callback)]() {
        try {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
            if (ec) {
                callback(AsyncResult<void>::error_result(
                    "Failed to remove directory: " + path + " - " + ec.message()));
                return;
            }
            callback(AsyncResult<void>::success_result());
        } catch (const std::exception& e) {
            callback(AsyncResult<void>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncListDirectory(T&& path,
                                  std::function<void(AsyncResult<std::vector<std::filesystem::path>>)> callback) {
    executeAsync([path = toString(std::forward<T>(path)), callback = std::move(callback)]() {
        try {
            std::vector<std::filesystem::path> entries;
            std::error_code ec;

            for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
                if (ec) {
                    callback(AsyncResult<std::vector<std::filesystem::path>>::error_result(
                        "Failed to iterate directory: " + path + " - " + ec.message()));
                    return;
                }
                entries.push_back(entry.path());
            }

            callback(AsyncResult<std::vector<std::filesystem::path>>::success_result(std::move(entries)));
        } catch (const std::exception& e) {
            callback(AsyncResult<std::vector<std::filesystem::path>>::error_result(e.what()));
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
            std::filesystem::copy_file(src, dest,
                std::filesystem::copy_options::overwrite_existing, ec);

            if (ec) {
                callback(AsyncResult<void>::error_result(
                    "Failed to copy file from " + src + " to " + dest + ": " + ec.message()));
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
                    "Failed to move file from " + src + " to " + dest + ": " + ec.message()));
                return;
            }

            callback(AsyncResult<void>::success_result());
        } catch (const std::exception& e) {
            callback(AsyncResult<void>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncReadWithTimeout(T&& filename, std::chrono::milliseconds timeout,
                                    std::function<void(AsyncResult<std::string>)> callback) {
    auto filename_str = toString(std::forward<T>(filename));

    // Launch the read operation asynchronously
    auto read_future = std::async(std::launch::async, [filename_str]() -> AsyncResult<std::string> {
        try {
            std::ifstream file(filename_str, std::ios::binary);
            if (!file) {
                return AsyncResult<std::string>::error_result("Failed to open file: " + filename_str);
            }

            file.seekg(0, std::ios::end);
            auto size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::string content(size, '\0');
            if (!file.read(content.data(), size)) {
                return AsyncResult<std::string>::error_result("Failed to read file: " + filename_str);
            }

            return AsyncResult<std::string>::success_result(std::move(content));
        } catch (const std::exception& e) {
            return AsyncResult<std::string>::error_result(e.what());
        }
    });

    // Wait for completion with timeout
    executeAsync([read_future = std::move(read_future), timeout, callback = std::move(callback)]() mutable {
        if (read_future.wait_for(timeout) == std::future_status::timeout) {
            callback(AsyncResult<std::string>::error_result("Read operation timed out"));
        } else {
            callback(read_future.get());
        }
    });
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<std::string>> AsyncFile::readFile(T&& filename) {
    std::promise<AsyncResult<std::string>> promise;
    auto future = promise.get_future();

    asyncRead(std::forward<T>(filename), [&promise](AsyncResult<std::string> result) {
        promise.set_value(std::move(result));
    });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<void>> AsyncFile::writeFile(T&& filename, std::span<const char> content) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    asyncWrite(std::forward<T>(filename), content, [&promise](AsyncResult<void> result) {
        promise.set_value(std::move(result));
    });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<std::vector<std::filesystem::path>>> AsyncFile::listDirectory(T&& path) {
    std::promise<AsyncResult<std::vector<std::filesystem::path>>> promise;
    auto future = promise.get_future();

    asyncListDirectory(std::forward<T>(path), [&promise](AsyncResult<std::vector<std::filesystem::path>> result) {
        promise.set_value(std::move(result));
    });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<void>> AsyncFile::deleteFile(T&& filename) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    asyncDelete(std::forward<T>(filename), [&promise](AsyncResult<void> result) {
        promise.set_value(std::move(result));
    });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<std::filesystem::file_status>> AsyncFile::getFileStatus(T&& filename) {
    std::promise<AsyncResult<std::filesystem::file_status>> promise;
    auto future = promise.get_future();

    asyncStat(std::forward<T>(filename), [&promise](AsyncResult<std::filesystem::file_status> result) {
        promise.set_value(std::move(result));
    });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<bool>> AsyncFile::fileExists(T&& filename) {
    std::promise<AsyncResult<bool>> promise;
    auto future = promise.get_future();

    asyncExists(std::forward<T>(filename), [&promise](AsyncResult<bool> result) {
        promise.set_value(std::move(result));
    });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<void>> AsyncFile::changePermissions(T&& filename, std::filesystem::perms perms) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    asyncChangePermissions(std::forward<T>(filename), perms, [&promise](AsyncResult<void> result) {
        promise.set_value(std::move(result));
    });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<void>> AsyncFile::createDirectory(T&& path) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    asyncCreateDirectory(std::forward<T>(path), [&promise](AsyncResult<void> result) {
        promise.set_value(std::move(result));
    });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<void>> AsyncFile::removeDirectory(T&& path) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    asyncRemoveDirectory(std::forward<T>(path), [&promise](AsyncResult<void> result) {
        promise.set_value(std::move(result));
    });

    co_return future.get();
}

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

// Implementation of streaming functions
template <PathString T>
void AsyncFile::asyncStreamRead(T&& filename, size_t chunk_size,
                               std::function<void(AsyncResult<std::string>)> chunk_callback,
                               std::function<void(AsyncResult<void>)> completion_callback) {
    executeAsync([filename = toString(std::forward<T>(filename)), chunk_size,
                  chunk_callback = std::move(chunk_callback),
                  completion_callback = std::move(completion_callback)]() mutable {
        try {
            std::ifstream file(filename, std::ios::binary);
            if (!file) {
                completion_callback(AsyncResult<void>::error_result("Failed to open file: " + filename));
                return;
            }

            std::string chunk(chunk_size, '\0');
            while (file.read(chunk.data(), chunk_size) || file.gcount() > 0) {
                chunk.resize(file.gcount());
                chunk_callback(AsyncResult<std::string>::success_result(std::string(chunk)));
                chunk.resize(chunk_size);
            }

            completion_callback(AsyncResult<void>::success_result());
        } catch (const std::exception& e) {
            completion_callback(AsyncResult<void>::error_result(e.what()));
        }
    });
}

template <PathString T>
void AsyncFile::asyncStreamWrite(T&& filename, std::span<const char> data,
                                size_t chunk_size, std::function<void(AsyncResult<void>)> callback) {
    executeAsync([filename = toString(std::forward<T>(filename)),
                  data = std::string(data.begin(), data.end()), chunk_size,
                  callback = std::move(callback)]() mutable {
        try {
            std::ofstream file(filename, std::ios::binary);
            if (!file) {
                callback(AsyncResult<void>::error_result("Failed to open file for writing: " + filename));
                return;
            }

            size_t written = 0;
            while (written < data.size()) {
                size_t to_write = std::min(chunk_size, data.size() - written);
                if (!file.write(data.data() + written, to_write)) {
                    callback(AsyncResult<void>::error_result("Failed to write chunk to file: " + filename));
                    return;
                }
                written += to_write;
            }

            callback(AsyncResult<void>::success_result());
        } catch (const std::exception& e) {
            callback(AsyncResult<void>::error_result(e.what()));
        }
    });
}

}  // namespace atom::async::io

#endif  // ATOM_IO_ASYNC_ASYNC_IO_HPP

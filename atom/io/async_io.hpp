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
#include <utility>
#include <vector>

#include <asio.hpp>

namespace atom::async::io {

// Concept for valid path string
template <typename T>
concept PathString = std::convertible_to<T, std::string> ||
                     std::convertible_to<T, std::filesystem::path>;

// Result type for async operations
template <typename T>
struct AsyncResult {
    bool success{false};
    std::string error_message;
    T value{};
};

template <>
struct AsyncResult<void> {
    bool success{false};
    std::string error_message;
};

// Forward declaration for coroutine task
template <typename T>
class [[nodiscard]] Task;

/**
 * @brief Class for performing asynchronous file operations.
 */
class AsyncFile {
public:
    /**
     * @brief Constructs an AsyncFile object.
     * @param io_context The ASIO I/O context.
     */
    explicit AsyncFile(asio::io_context& io_context) noexcept;

    /**
     * @brief Asynchronously reads the content of a file.
     * @param filename The name of the file to read.
     * @param callback The callback function to call with the file content.
     * @throws std::invalid_argument If the filename is empty.
     */
    void asyncRead(
        PathString auto&& filename,
        const std::function<void(AsyncResult<std::string>)>& callback);

    /**
     * @brief Asynchronously writes content to a file.
     * @param filename The name of the file to write to.
     * @param content The content to write to the file.
     * @param callback The callback function to call with the result of the
     * operation.
     * @throws std::invalid_argument If the filename is empty.
     */
    void asyncWrite(PathString auto&& filename, std::span<const char> content,
                    const std::function<void(AsyncResult<void>)>& callback);

    /**
     * @brief Asynchronously deletes a file.
     * @param filename The name of the file to delete.
     * @param callback The callback function to call with the result of the
     * operation.
     * @throws std::invalid_argument If the filename is empty.
     */
    void asyncDelete(PathString auto&& filename,
                     const std::function<void(AsyncResult<void>)>& callback);

    /**
     * @brief Asynchronously copies a file.
     * @param src The source file path.
     * @param dest The destination file path.
     * @param callback The callback function to call with the result of the
     * operation.
     * @throws std::invalid_argument If either path is empty.
     */
    void asyncCopy(PathString auto&& src, PathString auto&& dest,
                   const std::function<void(AsyncResult<void>)>& callback);

    /**
     * @brief Asynchronously reads the content of a file with a timeout.
     * @param filename The name of the file to read.
     * @param timeoutMs The timeout in milliseconds.
     * @param callback The callback function to call with the file content.
     * @throws std::invalid_argument If the filename is empty or timeout is
     * invalid.
     */
    void asyncReadWithTimeout(
        PathString auto&& filename, std::chrono::milliseconds timeoutMs,
        const std::function<void(AsyncResult<std::string>)>& callback);

    /**
     * @brief Asynchronously reads the content of multiple files.
     * @param files The list of files to read.
     * @param callback The callback function to call with the content of the
     * files.
     * @throws std::invalid_argument If the file list is empty.
     */
    void asyncBatchRead(
        const std::vector<std::string>& files,
        const std::function<void(AsyncResult<std::vector<std::string>>)>&
            callback);

    /**
     * @brief Asynchronously retrieves the status of a file.
     * @param filename The name of the file.
     * @param callback The callback function to call with the file status.
     * @throws std::invalid_argument If the filename is empty.
     */
    void asyncStat(
        PathString auto&& filename,
        const std::function<void(AsyncResult<std::filesystem::file_status>)>&
            callback);

    /**
     * @brief Asynchronously moves a file.
     * @param src The source file path.
     * @param dest The destination file path.
     * @param callback The callback function to call with the result of the
     * operation.
     * @throws std::invalid_argument If either path is empty.
     */
    void asyncMove(PathString auto&& src, PathString auto&& dest,
                   const std::function<void(AsyncResult<void>)>& callback);

    /**
     * @brief Asynchronously changes the permissions of a file.
     * @param filename The name of the file.
     * @param perms The new permissions.
     * @param callback The callback function to call with the result of the
     * operation.
     * @throws std::invalid_argument If the filename is empty.
     */
    void asyncChangePermissions(
        PathString auto&& filename, std::filesystem::perms perms,
        const std::function<void(AsyncResult<void>)>& callback);

    /**
     * @brief Asynchronously creates a directory.
     * @param path The path of the directory to create.
     * @param callback The callback function to call with the result of the
     * operation.
     * @throws std::invalid_argument If the path is empty.
     */
    void asyncCreateDirectory(
        PathString auto&& path,
        const std::function<void(AsyncResult<void>)>& callback);

    /**
     * @brief Asynchronously checks if a file exists.
     * @param filename The name of the file.
     * @param callback The callback function to call with the result of the
     * check.
     * @throws std::invalid_argument If the filename is empty.
     */
    void asyncExists(PathString auto&& filename,
                     const std::function<void(AsyncResult<bool>)>& callback);

    /**
     * @brief Coroutine-based asynchronous file read.
     * @param filename The name of the file to read.
     * @return A Task that will complete with the file content.
     */
    [[nodiscard]] Task<AsyncResult<std::string>> readFile(
        PathString auto&& filename);

    /**
     * @brief Coroutine-based asynchronous file write.
     * @param filename The name of the file to write to.
     * @param content The content to write.
     * @return A Task that will complete when the operation is done.
     */
    [[nodiscard]] Task<AsyncResult<void>> writeFile(
        PathString auto&& filename, std::span<const char> content);

private:
    asio::io_context& io_context_;  ///< The ASIO I/O context.
    std::shared_ptr<asio::steady_timer>
        timer_;  ///< Smart pointer to timer for operations.

    static constexpr int kSimulateSlowReadingMs =
        100;  ///< Simulated slow reading time in milliseconds.

    /**
     * @brief Validates a file path.
     * @param path The path to validate.
     * @return True if the path is valid, false otherwise.
     */
    static bool validatePath(const std::string& path) noexcept;
};

/**
 * @brief Class for performing asynchronous directory operations.
 */
class AsyncDirectory {
public:
    /**
     * @brief Constructs an AsyncDirectory object.
     * @param io_context The ASIO I/O context.
     */
    explicit AsyncDirectory(asio::io_context& io_context) noexcept;

    /**
     * @brief Asynchronously creates a directory.
     * @param path The path of the directory to create.
     * @param callback The callback function to call with the result of the
     * operation.
     * @throws std::invalid_argument If the path is empty.
     */
    void asyncCreate(PathString auto&& path,
                     const std::function<void(AsyncResult<void>)>& callback);

    /**
     * @brief Asynchronously removes a directory.
     * @param path The path of the directory to remove.
     * @param callback The callback function to call with the result of the
     * operation.
     * @throws std::invalid_argument If the path is empty.
     */
    void asyncRemove(PathString auto&& path,
                     const std::function<void(AsyncResult<void>)>& callback);

    /**
     * @brief Asynchronously lists the contents of a directory.
     * @param path The path of the directory.
     * @param callback The callback function to call with the list of contents.
     * @throws std::invalid_argument If the path is empty.
     */
    void asyncListContents(
        PathString auto&& path,
        const std::function<
            void(AsyncResult<std::vector<std::filesystem::path>>)>& callback);

    /**
     * @brief Asynchronously checks if a directory exists.
     * @param path The path of the directory.
     * @param callback The callback function to call with the result of the
     * check.
     * @throws std::invalid_argument If the path is empty.
     */
    void asyncExists(PathString auto&& path,
                     const std::function<void(AsyncResult<bool>)>& callback);

    /**
     * @brief Coroutine-based asynchronous directory listing.
     * @param path The path of the directory to list.
     * @return A Task that will complete with the directory contents.
     */
    [[nodiscard]] Task<AsyncResult<std::vector<std::filesystem::path>>>
    listContents(PathString auto&& path);

private:
    asio::io_context& io_context_;  ///< The ASIO I/O context.

    /**
     * @brief Validates a directory path.
     * @param path The path to validate.
     * @return True if the path is valid, false otherwise.
     */
    static bool validateDirectoryPath(const std::string& path) noexcept;
};

// Simple coroutine Task implementation
template <typename T>
class [[nodiscard]] Task {
public:
    // Define promise type for the coroutine
    struct promise_type {
        std::promise<T> promise;

        Task get_return_object() noexcept { return Task(promise.get_future()); }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_value(T value) noexcept {
            promise.set_value(std::move(value));
        }

        void unhandled_exception() noexcept {
            try {
                std::rethrow_exception(std::current_exception());
            } catch (const std::exception& e) {
                // Create a failed result with the exception message
                T failed_result;
                if constexpr (std::is_same_v<T, AsyncResult<void>>) {
                    failed_result.success = false;
                    failed_result.error_message = e.what();
                } else {
                    failed_result.success = false;
                    failed_result.error_message = e.what();
                }
                promise.set_value(std::move(failed_result));
            }
        }
    };

    explicit Task(std::future<T> future) noexcept
        : future_(std::move(future)) {}

    T get() { return future_.get(); }
    bool is_ready() const noexcept {
        return future_.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready;
    }

private:
    std::future<T> future_;
};

}  // namespace atom::async::io

#endif  // ATOM_IO_ASYNC_IO_HPP

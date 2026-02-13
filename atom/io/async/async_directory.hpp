#ifndef ATOM_IO_ASYNC_ASYNC_DIRECTORY_HPP
#define ATOM_IO_ASYNC_ASYNC_DIRECTORY_HPP

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

#include "async_file.hpp"
#include "async_types.hpp"

namespace atom::io::async {

/**
 * @brief Asynchronous directory operations
 *
 * Provides async directory create/remove/list operations using the same
 * execution infrastructure as AsyncFile.
 */
class AsyncDirectoryOps {
public:
#ifdef ATOM_USE_ASIO
    /**
     * @brief Constructs with ASIO context
     * @param io_context The ASIO I/O context
     * @param context Optional async context for cancellation support
     */
    explicit AsyncDirectoryOps(
        asio::io_context& io_context,
        std::shared_ptr<AsyncContext> context = nullptr) noexcept;
#else
    /**
     * @brief Constructs with thread pool
     * @param context Optional async context for cancellation support
     */
    explicit AsyncDirectoryOps(
        std::shared_ptr<AsyncContext> context = nullptr) noexcept;
#endif

    /**
     * @brief Asynchronously creates a directory
     * @param path Path of the directory to create
     * @param callback Callback function for the result
     */
    void asyncCreateDirectory(PathString auto&& path,
                              std::function<void(AsyncResult<void>)> callback);

    /**
     * @brief Asynchronously removes a directory
     * @param path Path of the directory to remove
     * @param callback Callback function for the result
     */
    void asyncRemoveDirectory(PathString auto&& path,
                              std::function<void(AsyncResult<void>)> callback);

    /**
     * @brief Asynchronously lists directory contents
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
    [[nodiscard]] atom::async::Task<
        AsyncResult<std::vector<std::filesystem::path>>>
    listDirectory(PathString auto&& path);

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
    std::shared_ptr<AsyncFile> file_impl_;
};

/**
 * @brief Legacy AsyncDirectory interface for backward compatibility
 * @deprecated Use AsyncDirectoryOps or AsyncFile methods instead
 */
class [[deprecated(
    "Use AsyncDirectoryOps for directory operations")]] AsyncDirectory {
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

// Template implementations for AsyncDirectoryOps

template <PathString T>
void AsyncDirectoryOps::asyncCreateDirectory(
    T&& path, std::function<void(AsyncResult<void>)> callback) {
    file_impl_->executeAsync(
        [path = AsyncFile::toString(std::forward<T>(path)),
         callback = std::move(callback)]() {
            try {
                std::error_code ec;
                std::filesystem::create_directories(path, ec);
                if (ec) {
                    callback(AsyncResult<void>::error_result(
                        "Failed to create directory: " + path + " - " +
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
void AsyncDirectoryOps::asyncRemoveDirectory(
    T&& path, std::function<void(AsyncResult<void>)> callback) {
    file_impl_->executeAsync(
        [path = AsyncFile::toString(std::forward<T>(path)),
         callback = std::move(callback)]() {
            try {
                std::error_code ec;
                std::filesystem::remove_all(path, ec);
                if (ec) {
                    callback(AsyncResult<void>::error_result(
                        "Failed to remove directory: " + path + " - " +
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
void AsyncDirectoryOps::asyncListDirectory(
    T&& path,
    std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>
        callback) {
    file_impl_->executeAsync(
        [path = AsyncFile::toString(std::forward<T>(path)),
         callback = std::move(callback)]() {
            try {
                std::vector<std::filesystem::path> entries;
                std::error_code ec;

                for (const auto& entry :
                     std::filesystem::directory_iterator(path, ec)) {
                    if (ec) {
                        callback(
                            AsyncResult<std::vector<std::filesystem::path>>::
                                error_result(
                                    "Failed to iterate directory: " + path +
                                    " - " + ec.message()));
                        return;
                    }
                    entries.push_back(entry.path());
                }

                callback(AsyncResult<
                         std::vector<std::filesystem::path>>::success_result(
                    std::move(entries)));
            } catch (const std::exception& e) {
                callback(
                    AsyncResult<std::vector<std::filesystem::path>>::
                        error_result(e.what()));
            }
        });
}

template <PathString T>
[[nodiscard]] atom::async::Task<
    AsyncResult<std::vector<std::filesystem::path>>>
AsyncDirectoryOps::listDirectory(T&& path) {
    std::promise<AsyncResult<std::vector<std::filesystem::path>>> promise;
    auto future = promise.get_future();

    asyncListDirectory(
        std::forward<T>(path),
        [&promise](AsyncResult<std::vector<std::filesystem::path>> result) {
            promise.set_value(std::move(result));
        });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<void>>
AsyncDirectoryOps::createDirectory(T&& path) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    asyncCreateDirectory(std::forward<T>(path),
                         [&promise](AsyncResult<void> result) {
                             promise.set_value(std::move(result));
                         });

    co_return future.get();
}

template <PathString T>
[[nodiscard]] atom::async::Task<AsyncResult<void>>
AsyncDirectoryOps::removeDirectory(T&& path) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    asyncRemoveDirectory(std::forward<T>(path),
                         [&promise](AsyncResult<void> result) {
                             promise.set_value(std::move(result));
                         });

    co_return future.get();
}

}  // namespace atom::io::async

#endif  // ATOM_IO_ASYNC_ASYNC_DIRECTORY_HPP

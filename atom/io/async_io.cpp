#include "async_io.hpp"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <mutex>

#include "atom/io/io.hpp"
#include "atom/log/loguru.hpp"

namespace atom::async::io {

#ifndef ATOM_USE_ASIO
ThreadPool::ThreadPool(size_t threads) : stop_(false) {
    for (size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(
                        lock, [this] { return stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty())
                        return;
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread& worker : workers_) {
        worker.join();
    }
}

template <typename F>
void ThreadPool::post(F&& func) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_)
            throw std::runtime_error("ThreadPool is stopped");
        tasks_.emplace(std::forward<F>(func));
    }
    condition_.notify_one();
}
#endif

#ifdef ATOM_USE_ASIO
AsyncFile::AsyncFile(asio::io_context& io_context) noexcept
    : io_context_(io_context),
      timer_(std::make_shared<asio::steady_timer>(io_context)) {}
#else
AsyncFile::AsyncFile() noexcept
    : thread_pool_(std::thread::hardware_concurrency()) {}
#endif

bool AsyncFile::validatePath(const std::string& path) noexcept {
    if (path.empty()) {
        return false;
    }

    try {
        std::filesystem::path fs_path(path);
        return !fs_path.empty();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Path validation failed: {}", e.what());
        return false;
    }
}

#ifndef ATOM_USE_ASIO
template <typename F>
void AsyncFile::scheduleTimeout(std::chrono::milliseconds timeout,
                                F&& callback) {
    std::thread([timeout, callback = std::forward<F>(callback)]() {
        std::this_thread::sleep_for(timeout);
        callback();
    }).detach();
}
#endif

template <PathString T>
void AsyncFile::asyncRead(
    T&& filename,
    const std::function<void(AsyncResult<std::string>)>& callback) {
    std::string file_path = std::forward<T>(filename);

    if (!validatePath(file_path)) {
        if (callback) {
            AsyncResult<std::string> result;
            result.success = false;
            result.error_message = "Invalid filename";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([file_path, callback]() {
#else
    thread_pool_.post([file_path, callback]() {
#endif
        AsyncResult<std::string> result;

        try {
            std::ifstream file(file_path, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                result.success = false;
                result.error_message = "Failed to open file";
                callback(result);
                return;
            }

            auto size = file.tellg();
            if (size < 0 || size > static_cast<std::streamoff>(
                                       std::numeric_limits<size_t>::max())) {
                result.success = false;
                result.error_message = "Invalid file size";
                callback(result);
                return;
            }

            file.seekg(0);
            result.value.resize(static_cast<size_t>(size));

            if (size > 0) {
                file.read(result.value.data(), size);
                if (!file) {
                    result.success = false;
                    result.error_message = "Failed to read file content";
                    callback(result);
                    return;
                }
            }

            file.close();
            result.success = true;
            callback(result);
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

template <PathString T>
void AsyncFile::asyncWrite(
    T&& filename, std::span<const char> content,
    const std::function<void(AsyncResult<void>)>& callback) {
    std::string file_path = std::forward<T>(filename);

    if (!validatePath(file_path)) {
        if (callback) {
            AsyncResult<void> result;
            result.success = false;
            result.error_message = "Invalid filename";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

    auto content_copy =
        std::make_shared<std::vector<char>>(content.begin(), content.end());

#ifdef ATOM_USE_ASIO
    io_context_.post([file_path, content_copy, callback]() {
#else
    thread_pool_.post([file_path, content_copy, callback]() {
#endif
        AsyncResult<void> result;

        try {
            auto dir_path = std::filesystem::path(file_path).parent_path();
            if (!dir_path.empty()) {
                std::filesystem::create_directories(dir_path);
            }

            std::ofstream file(file_path, std::ios::binary);
            if (!file.is_open()) {
                result.success = false;
                result.error_message = "Failed to open file";
                callback(result);
                return;
            }

            if (!content_copy->empty()) {
                file.write(content_copy->data(),
                           static_cast<std::streamsize>(content_copy->size()));
                if (!file) {
                    result.success = false;
                    result.error_message = "Failed to write to file";
                    callback(result);
                    return;
                }
            }

            file.close();
            result.success = true;
            callback(result);
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

template <PathString T>
void AsyncFile::asyncDelete(
    T&& filename, const std::function<void(AsyncResult<void>)>& callback) {
    std::string file_path = std::forward<T>(filename);

    if (!validatePath(file_path)) {
        if (callback) {
            AsyncResult<void> result;
            result.success = false;
            result.error_message = "Invalid filename";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([file_path, callback]() {
#else
    thread_pool_.post([file_path, callback]() {
#endif
        AsyncResult<void> result;

        try {
            if (atom::io::removeFile(file_path)) {
                result.success = true;
                callback(result);
            } else {
                result.success = false;
                result.error_message = "Failed to delete file";
                callback(result);
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

template <PathString T1, PathString T2>
void AsyncFile::asyncCopy(
    T1&& src, T2&& dest,
    const std::function<void(AsyncResult<void>)>& callback) {
    std::string src_path = std::forward<T1>(src);
    std::string dest_path = std::forward<T2>(dest);

    if (!validatePath(src_path) || !validatePath(dest_path)) {
        if (callback) {
            AsyncResult<void> result;
            result.success = false;
            result.error_message = "Invalid source or destination path";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([src_path, dest_path, callback]() {
#else
    thread_pool_.post([src_path, dest_path, callback]() {
#endif
        AsyncResult<void> result;

        try {
            auto dir_path = std::filesystem::path(dest_path).parent_path();
            if (!dir_path.empty()) {
                std::filesystem::create_directories(dir_path);
            }

            if (atom::io::copyFile(src_path, dest_path)) {
                result.success = true;
                callback(result);
            } else {
                result.success = false;
                result.error_message = "Failed to copy file";
                callback(result);
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

template <PathString T>
void AsyncFile::asyncReadWithTimeout(
    T&& filename, std::chrono::milliseconds timeoutMs,
    const std::function<void(AsyncResult<std::string>)>& callback) {
    std::string file_path = std::forward<T>(filename);

    if (!validatePath(file_path)) {
        if (callback) {
            AsyncResult<std::string> result;
            result.success = false;
            result.error_message = "Invalid filename";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

    if (timeoutMs.count() <= 0) {
        if (callback) {
            AsyncResult<std::string> result;
            result.success = false;
            result.error_message = "Invalid timeout value";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

    auto completed = std::make_shared<std::atomic<bool>>(false);

    asyncRead(file_path,
              [completed, callback](AsyncResult<std::string> result) {
                  if (!completed->exchange(true)) {
                      callback(result);
                  }
              });

#ifdef ATOM_USE_ASIO
    auto timer = std::make_shared<asio::steady_timer>(io_context_);
    timer->expires_after(timeoutMs);
    timer->async_wait(
        [completed, file_path, callback](const std::error_code& errorCode) {
            if (!completed->exchange(true) && !errorCode) {
                AsyncResult<std::string> result;
                result.success = false;
                result.error_message = "Operation timed out";
                callback(result);
            }
        });
#else
    scheduleTimeout(timeoutMs, [completed, file_path, callback]() {
        if (!completed->exchange(true)) {
            AsyncResult<std::string> result;
            result.success = false;
            result.error_message = "Operation timed out";
            callback(result);
        }
    });
#endif
}

void AsyncFile::asyncBatchRead(
    const std::vector<std::string>& files,
    const std::function<void(AsyncResult<std::vector<std::string>>)>&
        callback) {
    if (files.empty()) {
        if (callback) {
            AsyncResult<std::vector<std::string>> result;
            result.success = false;
            result.error_message = "Empty file list";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

    bool all_valid =
        std::all_of(files.begin(), files.end(),
                    [](const std::string& file) { return validatePath(file); });

    if (!all_valid) {
        if (callback) {
            AsyncResult<std::vector<std::string>> result;
            result.success = false;
            result.error_message = "One or more invalid file paths";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

    auto results = std::make_shared<std::vector<std::string>>(files.size());
    auto errors = std::make_shared<std::vector<std::string>>(files.size());
    auto mutex = std::make_shared<std::mutex>();
    auto remaining = std::make_shared<std::atomic<int>>(files.size());

    for (size_t i = 0; i < files.size(); ++i) {
        asyncRead(files[i], [results, errors, mutex, remaining, callback, i,
                             total_files = files.size()](
                                AsyncResult<std::string> result) {
            bool all_done = false;
            {
                std::lock_guard<std::mutex> lock(*mutex);
                if (result.success) {
                    (*results)[i] = std::move(result.value);
                } else {
                    (*errors)[i] = std::move(result.error_message);
                }
                all_done = (--(*remaining) == 0);
            }

            if (all_done) {
                AsyncResult<std::vector<std::string>> final_result;
                final_result.value = std::move(*results);

                std::string error_messages;
                for (size_t j = 0; j < total_files; ++j) {
                    if (!(*errors)[j].empty()) {
                        if (!error_messages.empty()) {
                            error_messages += "; ";
                        }
                        error_messages +=
                            "File " + std::to_string(j) + ": " + (*errors)[j];
                    }
                }

                if (error_messages.empty()) {
                    final_result.success = true;
                } else {
                    final_result.success = false;
                    final_result.error_message = std::move(error_messages);
                }

                callback(final_result);
            }
        });
    }
}

template <PathString T>
void AsyncFile::asyncStat(
    T&& filename,
    const std::function<void(AsyncResult<std::filesystem::file_status>)>&
        callback) {
    std::string file_path = std::forward<T>(filename);

    if (!validatePath(file_path)) {
        if (callback) {
            AsyncResult<std::filesystem::file_status> result;
            result.success = false;
            result.error_message = "Invalid filename";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([file_path, callback]() {
#else
    thread_pool_.post([file_path, callback]() {
#endif
        AsyncResult<std::filesystem::file_status> result;

        try {
            std::error_code ec;
            auto status = std::filesystem::status(file_path, ec);

            if (ec) {
                result.success = false;
                result.error_message =
                    "Failed to get file status: " + ec.message();
                callback(result);
                return;
            }

            result.value = status;
            result.success = true;
            callback(result);
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

template <PathString T1, PathString T2>
void AsyncFile::asyncMove(
    T1&& src, T2&& dest,
    const std::function<void(AsyncResult<void>)>& callback) {
    std::string src_path = std::forward<T1>(src);
    std::string dest_path = std::forward<T2>(dest);

    if (!validatePath(src_path) || !validatePath(dest_path)) {
        if (callback) {
            AsyncResult<void> result;
            result.success = false;
            result.error_message = "Invalid source or destination path";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([src_path, dest_path, callback]() {
#else
    thread_pool_.post([src_path, dest_path, callback]() {
#endif
        AsyncResult<void> result;

        try {
            auto dir_path = std::filesystem::path(dest_path).parent_path();
            if (!dir_path.empty()) {
                std::filesystem::create_directories(dir_path);
            }

            if (atom::io::moveFile(src_path, dest_path)) {
                result.success = true;
                callback(result);
            } else {
                result.success = false;
                result.error_message = "Failed to move file";
                callback(result);
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

template <PathString T>
void AsyncFile::asyncChangePermissions(
    T&& filename, std::filesystem::perms perms,
    const std::function<void(AsyncResult<void>)>& callback) {
    std::string file_path = std::forward<T>(filename);

    if (!validatePath(file_path)) {
        if (callback) {
            AsyncResult<void> result;
            result.success = false;
            result.error_message = "Invalid filename";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([file_path, perms, callback]() {
#else
    thread_pool_.post([file_path, perms, callback]() {
#endif
        AsyncResult<void> result;

        try {
            std::error_code errorCode;
            std::filesystem::permissions(file_path, perms, errorCode);
            if (!errorCode) {
                result.success = true;
                callback(result);
            } else {
                result.success = false;
                result.error_message =
                    "Failed to change file permissions: " + errorCode.message();
                callback(result);
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

template <PathString T>
void AsyncFile::asyncCreateDirectory(
    T&& path, const std::function<void(AsyncResult<void>)>& callback) {
    std::string dir_path = std::forward<T>(path);

    if (!validatePath(dir_path)) {
        if (callback) {
            AsyncResult<void> result;
            result.success = false;
            result.error_message = "Invalid directory path";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([dir_path, callback]() {
#else
    thread_pool_.post([dir_path, callback]() {
#endif
        AsyncResult<void> result;

        try {
            if (std::filesystem::create_directories(dir_path)) {
                result.success = true;
                callback(result);
            } else {
                if (std::filesystem::exists(dir_path) &&
                    std::filesystem::is_directory(dir_path)) {
                    result.success = true;
                    callback(result);
                } else {
                    result.success = false;
                    result.error_message = "Failed to create directory";
                    callback(result);
                }
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

template <PathString T>
void AsyncFile::asyncExists(
    T&& filename, const std::function<void(AsyncResult<bool>)>& callback) {
    std::string file_path = std::forward<T>(filename);

    if (!validatePath(file_path)) {
        if (callback) {
            AsyncResult<bool> result;
            result.success = false;
            result.error_message = "Invalid filename";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([file_path, callback]() {
#else
    thread_pool_.post([file_path, callback]() {
#endif
        AsyncResult<bool> result;

        try {
            std::error_code ec;
            result.value = std::filesystem::exists(file_path, ec);

            if (ec) {
                result.success = false;
                result.error_message =
                    "Error checking file existence: " + ec.message();
            } else {
                result.success = true;
            }

            callback(result);
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

Task<AsyncResult<std::string>> AsyncFile::readFile(PathString auto&& filename) {
    std::string file_path = std::forward<decltype(filename)>(filename);

    if (!validatePath(file_path)) {
        AsyncResult<std::string> result;
        result.success = false;
        result.error_message = "Invalid filename";
        co_return result;
    }

    std::promise<AsyncResult<std::string>> promise;
    auto future = promise.get_future();

    asyncRead(file_path,
              [promise = std::make_shared<decltype(promise)>(std::move(
                   promise))](AsyncResult<std::string> result) mutable {
                  promise->set_value(std::move(result));
              });

    co_return future.get();
}

Task<AsyncResult<void>> AsyncFile::writeFile(PathString auto&& filename,
                                             std::span<const char> content) {
    std::string file_path = std::forward<decltype(filename)>(filename);

    if (!validatePath(file_path)) {
        AsyncResult<void> result;
        result.success = false;
        result.error_message = "Invalid filename";
        co_return result;
    }

    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    asyncWrite(file_path, content,
               [promise = std::make_shared<decltype(promise)>(
                    std::move(promise))](AsyncResult<void> result) mutable {
                   promise->set_value(std::move(result));
               });

    co_return future.get();
}

#ifdef ATOM_USE_ASIO
AsyncDirectory::AsyncDirectory(asio::io_context& io_context) noexcept
    : io_context_(io_context) {}
#else
AsyncDirectory::AsyncDirectory() noexcept
    : thread_pool_(std::thread::hardware_concurrency()) {}
#endif

bool AsyncDirectory::validateDirectoryPath(const std::string& path) noexcept {
    if (path.empty()) {
        return false;
    }

    try {
        std::filesystem::path fs_path(path);
        return !fs_path.empty();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Directory path validation failed: {}", e.what());
        return false;
    }
}

template <PathString T>
void AsyncDirectory::asyncCreate(
    T&& path, const std::function<void(AsyncResult<void>)>& callback) {
    std::string dir_path = std::forward<T>(path);

    if (!validateDirectoryPath(dir_path)) {
        if (callback) {
            AsyncResult<void> result;
            result.success = false;
            result.error_message = "Invalid directory path";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([dir_path, callback]() {
#else
    thread_pool_.post([dir_path, callback]() {
#endif
        AsyncResult<void> result;

        try {
            std::error_code ec;
            if (std::filesystem::create_directories(dir_path, ec)) {
                result.success = true;
                callback(result);
            } else if (ec) {
                result.success = false;
                result.error_message =
                    "Failed to create directory: " + ec.message();
                callback(result);
            } else {
                if (std::filesystem::exists(dir_path, ec) &&
                    std::filesystem::is_directory(dir_path, ec)) {
                    result.success = true;
                    callback(result);
                } else {
                    result.success = false;
                    result.error_message = "Failed to create directory";
                    callback(result);
                }
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

template <PathString T>
void AsyncDirectory::asyncRemove(
    T&& path, const std::function<void(AsyncResult<void>)>& callback) {
    std::string dir_path = std::forward<T>(path);

    if (!validateDirectoryPath(dir_path)) {
        if (callback) {
            AsyncResult<void> result;
            result.success = false;
            result.error_message = "Invalid directory path";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([dir_path, callback]() {
#else
    thread_pool_.post([dir_path, callback]() {
#endif
        AsyncResult<void> result;

        try {
            std::error_code ec;
            std::uintmax_t removed = std::filesystem::remove_all(dir_path, ec);

            if (ec) {
                result.success = false;
                result.error_message =
                    "Failed to remove directory: " + ec.message();
                callback(result);
            } else {
                result.success = true;
                callback(result);
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

template <PathString T>
void AsyncDirectory::asyncListContents(
    T&& path,
    const std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>&
        callback) {
    std::string dir_path = std::forward<T>(path);

    if (!validateDirectoryPath(dir_path)) {
        if (callback) {
            AsyncResult<std::vector<std::filesystem::path>> result;
            result.success = false;
            result.error_message = "Invalid directory path";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([dir_path, callback]() {
#else
    thread_pool_.post([dir_path, callback]() {
#endif
        AsyncResult<std::vector<std::filesystem::path>> result;

        try {
            std::error_code ec;

            if (!std::filesystem::exists(dir_path, ec) ||
                !std::filesystem::is_directory(dir_path, ec)) {
                result.success = false;
                result.error_message =
                    "Directory does not exist or is not a directory";
                callback(result);
                return;
            }

            for (const auto& entry :
                 std::filesystem::directory_iterator(dir_path, ec)) {
                if (ec) {
                    result.success = false;
                    result.error_message =
                        "Error during directory iteration: " + ec.message();
                    callback(result);
                    return;
                }
                result.value.push_back(entry.path());
            }

            result.success = true;
            callback(result);
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

template <PathString T>
void AsyncDirectory::asyncExists(
    T&& path, const std::function<void(AsyncResult<bool>)>& callback) {
    std::string dir_path = std::forward<T>(path);

    if (!validateDirectoryPath(dir_path)) {
        if (callback) {
            AsyncResult<bool> result;
            result.success = false;
            result.error_message = "Invalid directory path";
#ifdef ATOM_USE_ASIO
            io_context_.post([result, callback]() { callback(result); });
#else
            thread_pool_.post([result, callback]() { callback(result); });
#endif
        }
        return;
    }

#ifdef ATOM_USE_ASIO
    io_context_.post([dir_path, callback]() {
#else
    thread_pool_.post([dir_path, callback]() {
#endif
        AsyncResult<bool> result;

        try {
            std::error_code ec;
            bool exists = std::filesystem::exists(dir_path, ec) &&
                          std::filesystem::is_directory(dir_path, ec);

            if (ec) {
                result.success = false;
                result.error_message =
                    "Error checking directory existence: " + ec.message();
            } else {
                result.value = exists;
                result.success = true;
            }

            callback(result);
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Exception: ") + e.what();
            callback(result);
        }
    });
}

Task<AsyncResult<std::vector<std::filesystem::path>>>
AsyncDirectory::listContents(PathString auto&& path) {
    std::string dir_path = std::forward<decltype(path)>(path);

    if (!validateDirectoryPath(dir_path)) {
        AsyncResult<std::vector<std::filesystem::path>> result;
        result.success = false;
        result.error_message = "Invalid directory path";
        co_return result;
    }

    std::promise<AsyncResult<std::vector<std::filesystem::path>>> promise;
    auto future = promise.get_future();

    asyncListContents(
        dir_path,
        [promise = std::make_shared<decltype(promise)>(std::move(promise))](
            AsyncResult<std::vector<std::filesystem::path>> result) mutable {
            promise->set_value(std::move(result));
        });

    co_return future.get();
}

// Explicit template instantiations to prevent linker errors
template void AsyncFile::asyncRead(
    const std::string&, const std::function<void(AsyncResult<std::string>)>&);
template void AsyncFile::asyncRead(
    std::string&&, const std::function<void(AsyncResult<std::string>)>&);
template void AsyncFile::asyncWrite(
    const std::string&, std::span<const char>,
    const std::function<void(AsyncResult<void>)>&);
template void AsyncFile::asyncWrite(
    std::string&&, std::span<const char>,
    const std::function<void(AsyncResult<void>)>&);
template void AsyncFile::asyncDelete(
    const std::string&, const std::function<void(AsyncResult<void>)>&);
template void AsyncFile::asyncDelete(
    std::string&&, const std::function<void(AsyncResult<void>)>&);
template void AsyncFile::asyncCopy(
    const std::string&, const std::string&,
    const std::function<void(AsyncResult<void>)>&);
template void AsyncFile::asyncCopy(
    std::string&&, std::string&&,
    const std::function<void(AsyncResult<void>)>&);
template void AsyncFile::asyncReadWithTimeout(
    const std::string&, std::chrono::milliseconds,
    const std::function<void(AsyncResult<std::string>)>&);
template void AsyncFile::asyncStat(
    const std::string&,
    const std::function<void(AsyncResult<std::filesystem::file_status>)>&);
template void AsyncFile::asyncMove(
    const std::string&, const std::string&,
    const std::function<void(AsyncResult<void>)>&);
template void AsyncFile::asyncChangePermissions(
    const std::string&, std::filesystem::perms,
    const std::function<void(AsyncResult<void>)>&);
template void AsyncFile::asyncCreateDirectory(
    const std::string&, const std::function<void(AsyncResult<void>)>&);
template void AsyncFile::asyncExists(
    const std::string&, const std::function<void(AsyncResult<bool>)>&);

template void AsyncDirectory::asyncCreate(
    const std::string&, const std::function<void(AsyncResult<void>)>&);
template void AsyncDirectory::asyncRemove(
    const std::string&, const std::function<void(AsyncResult<void>)>&);
template void AsyncDirectory::asyncListContents(
    const std::string&, const std::function<void(
                            AsyncResult<std::vector<std::filesystem::path>>)>&);
template void AsyncDirectory::asyncExists(
    const std::string&, const std::function<void(AsyncResult<bool>)>&);

template Task<AsyncResult<std::string>> AsyncFile::readFile(const std::string&);
template Task<AsyncResult<std::string>> AsyncFile::readFile(std::string&&);
template Task<AsyncResult<void>> AsyncFile::writeFile(const std::string&,
                                                      std::span<const char>);
template Task<AsyncResult<std::vector<std::filesystem::path>>>
AsyncDirectory::listContents(const std::string&);
template Task<AsyncResult<std::vector<std::filesystem::path>>>
AsyncDirectory::listContents(std::string&&);

}  // namespace atom::async::io

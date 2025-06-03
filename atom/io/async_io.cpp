#include "async_io.hpp"

#include <algorithm>
#include <string_view>

#include <spdlog/spdlog.h>

namespace atom::async::io {

#ifdef ATOM_USE_ASIO
AsyncFile::AsyncFile(asio::io_context& io_context,
                     std::shared_ptr<AsyncContext> context) noexcept
    : io_context_(io_context),
      timer_(std::make_shared<asio::steady_timer>(io_context)),
      context_(std::move(context)),
      logger_(spdlog::get("async_io") ? spdlog::get("async_io")
                                      : spdlog::default_logger()) {}
#else
AsyncFile::AsyncFile(std::shared_ptr<AsyncContext> context) noexcept
    : thread_pool_(std::make_shared<ThreadPool>(
          ThreadPool::Options::createHighPerformance())),
      context_(std::move(context)),
      logger_(spdlog::get("async_io") ? spdlog::get("async_io")
                                      : spdlog::default_logger()) {}
#endif

bool AsyncFile::validatePath(std::string_view path) noexcept {
    if (path.empty()) {
        return false;
    }

    try {
        std::filesystem::path fs_path(path);
        return !fs_path.empty();
    } catch (const std::exception& e) {
        spdlog::error("Path validation failed: {}", e.what());
        return false;
    }
}

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
    io_context_.post(std::forward<F>(operation));
#else
    thread_pool_->execute(std::forward<F>(operation));
#endif
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

void AsyncFile::asyncBatchRead(
    std::span<const std::string> files,
    std::function<void(AsyncResult<std::vector<std::string>>)> callback) {
    if (files.empty()) {
        if (callback) {
            auto result = AsyncResult<std::vector<std::string>>::error_result(
                "Empty file list");
            executeAsync([result = std::move(result),
                          callback = std::move(callback)]() mutable {
                callback(std::move(result));
            });
        }
        return;
    }

    bool all_valid = std::all_of(
        files.begin(), files.end(),
        [this](const std::string& file) { return validatePath(file); });

    if (!all_valid) {
        if (callback) {
            auto result = AsyncResult<std::vector<std::string>>::error_result(
                "One or more invalid file paths");
            executeAsync([result = std::move(result),
                          callback = std::move(callback)]() mutable {
                callback(std::move(result));
            });
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
                    auto final_result =
                        AsyncResult<std::vector<std::string>>::success_result(
                            std::move(*results));
                    callback(std::move(final_result));
                } else {
                    auto final_result =
                        AsyncResult<std::vector<std::string>>::error_result(
                            std::move(error_messages));
                    callback(std::move(final_result));
                }
            }
        });
    }
}

// Legacy AsyncDirectory implementation
#ifdef ATOM_USE_ASIO
AsyncDirectory::AsyncDirectory(asio::io_context& io_context) noexcept
    : file_impl_(std::make_unique<AsyncFile>(io_context)) {}
#else
AsyncDirectory::AsyncDirectory() noexcept
    : file_impl_(std::make_unique<AsyncFile>()) {}
#endif

// Template instantiations for common types
template std::string AsyncFile::toString<const std::string&>(
    const std::string&);
template std::string AsyncFile::toString<std::string&&>(std::string&&);

template void AsyncFile::executeAsync<std::function<void()>>(
    std::function<void()>&&);

#ifndef ATOM_USE_ASIO
template void AsyncFile::scheduleTimeout<std::function<void()>>(
    std::chrono::milliseconds, std::function<void()>&&);
#endif

}  // namespace atom::async::io

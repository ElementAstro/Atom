#include "async_io.hpp"

#include <algorithm>
#include <string_view>
#include <optional>
#include <fstream>
#include <thread>

#include <spdlog/spdlog.h>

namespace atom::async::io {

#ifdef ATOM_USE_ASIO
AsyncFile::AsyncFile(asio::io_context& io_context,
                     const AsyncIOConfig& config,
                     std::shared_ptr<AsyncContext> context) noexcept
    : io_context_(io_context),
      timer_(std::make_shared<asio::steady_timer>(io_context)),
      config_(config),
      context_(std::move(context)),
      logger_(spdlog::get("async_io") ? spdlog::get("async_io")
                                      : spdlog::default_logger()) {
    stats_.start_time = std::chrono::steady_clock::now();
    buffer_pool_.reserve(10); // Pre-allocate some buffer slots
}
#else
AsyncFile::AsyncFile(const AsyncIOConfig& config,
                     std::shared_ptr<AsyncContext> context) noexcept
    : thread_pool_(std::make_shared<ThreadPool>(
          ThreadPool::Options::createHighPerformance())),
      config_(config),
      context_(std::move(context)),
      logger_(spdlog::get("async_io") ? spdlog::get("async_io")
                                      : spdlog::default_logger()) {
    stats_.start_time = std::chrono::steady_clock::now();
    buffer_pool_.reserve(10); // Pre-allocate some buffer slots
}
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
        [](const std::string& file) { return validatePath(file); });

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

const AsyncIOStats& AsyncFile::getStats() const noexcept {
    return stats_;
}

void AsyncFile::resetStats() noexcept {
    stats_.reset();
}

void AsyncFile::updateConfig(const AsyncIOConfig& config) noexcept {
    config_ = config;
    // Clear buffer pool if buffer size changed
    if (config_.buffer_size != config.buffer_size) {
        std::lock_guard<std::mutex> lock(buffer_pool_mutex_);
        buffer_pool_.clear();
    }
}

std::optional<FileMetadata> AsyncFile::getFileMetadata(const std::string& path) const {
    if (!config_.enable_caching) {
        // Direct filesystem query without caching
        try {
            std::error_code ec;
            auto status = std::filesystem::status(path, ec);
            if (ec) {
                return std::nullopt;
            }

            FileMetadata metadata;
            metadata.status = status;
            metadata.size = std::filesystem::file_size(path, ec);
            if (ec) metadata.size = 0;
            metadata.last_write_time = std::filesystem::last_write_time(path, ec);
            metadata.cache_time = std::chrono::steady_clock::now();

            stats_.cache_misses++;
            return metadata;
        } catch (const std::exception& e) {
            logger_->error("Error getting file metadata for {}: {}", path, e.what());
            return std::nullopt;
        }
    }

    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = metadata_cache_.find(path);
    if (it != metadata_cache_.end() && it->second.isValid()) {
        stats_.cache_hits++;
        return it->second;
    }

    // Cache miss or expired entry
    try {
        std::error_code ec;
        auto status = std::filesystem::status(path, ec);
        if (ec) {
            return std::nullopt;
        }

        FileMetadata metadata;
        metadata.status = status;
        metadata.size = std::filesystem::file_size(path, ec);
        if (ec) metadata.size = 0;
        metadata.last_write_time = std::filesystem::last_write_time(path, ec);
        metadata.cache_time = std::chrono::steady_clock::now();

        // Update cache
        metadata_cache_[path] = metadata;
        stats_.cache_misses++;

        // Cleanup cache if it's getting too large
        if (metadata_cache_.size() > config_.cache_size_limit) {
            cleanupCache();
        }

        return metadata;
    } catch (const std::exception& e) {
        logger_->error("Error getting file metadata for {}: {}", path, e.what());
        return std::nullopt;
    }
}

std::vector<char> AsyncFile::getBuffer(std::size_t size) {
    std::lock_guard<std::mutex> lock(buffer_pool_mutex_);

    // Look for a buffer of appropriate size
    for (auto it = buffer_pool_.begin(); it != buffer_pool_.end(); ++it) {
        if (it->size() >= size) {
            auto buffer = std::move(*it);
            buffer_pool_.erase(it);
            buffer.resize(size);
            return buffer;
        }
    }

    // No suitable buffer found, create new one
    return std::vector<char>(size);
}

void AsyncFile::returnBuffer(std::vector<char>&& buffer) {
    if (buffer.empty()) return;

    std::lock_guard<std::mutex> lock(buffer_pool_mutex_);

    // Only keep a limited number of buffers to prevent memory bloat
    if (buffer_pool_.size() < 20) {
        buffer.clear();
        buffer.shrink_to_fit();
        buffer.resize(config_.buffer_size);
        buffer_pool_.push_back(std::move(buffer));
    }
}

void AsyncFile::cleanupCache() const {
    // Remove expired entries (called with cache_mutex_ already locked)
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::minutes(5); // Remove entries older than 5 minutes

    for (auto it = metadata_cache_.begin(); it != metadata_cache_.end();) {
        if (it->second.cache_time < cutoff) {
            it = metadata_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

template <typename F>
void AsyncFile::executeFileOperation(F&& operation, const std::string& operation_name) {
    if (context_ && context_->is_cancelled()) {
        return;
    }

    executeAsync([this, operation = std::forward<F>(operation), operation_name]() mutable {
        try {
            operation();
            stats_.operations_completed++;
        } catch (const std::exception& e) {
            stats_.operations_failed++;
            logger_->error("Error in {}: {}", operation_name, e.what());
        }
    });
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

#include "async_batch.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>

#include <spdlog/spdlog.h>

namespace atom::io::async {

#ifdef ATOM_USE_ASIO
AsyncBatchOps::AsyncBatchOps(asio::io_context& io_context,
                             std::shared_ptr<AsyncContext> context) noexcept
    : file_impl_(
          std::make_shared<AsyncFile>(io_context, std::move(context))) {}
#else
AsyncBatchOps::AsyncBatchOps(std::shared_ptr<AsyncContext> context) noexcept
    : file_impl_(std::make_shared<AsyncFile>(std::move(context))) {}
#endif

void AsyncBatchOps::asyncBatchRead(
    std::span<const std::string> files,
    std::function<void(AsyncResult<std::vector<std::string>>)> callback) {
    if (files.empty()) {
        if (callback) {
            auto result = AsyncResult<std::vector<std::string>>::error_result(
                "Empty file list");
            file_impl_->executeAsync([result = std::move(result),
                                      callback = std::move(callback)]() mutable {
                callback(std::move(result));
            });
        }
        return;
    }

    bool all_valid = std::all_of(
        files.begin(), files.end(),
        [](const std::string& file) { return AsyncFile::validatePath(file); });

    if (!all_valid) {
        if (callback) {
            auto result = AsyncResult<std::vector<std::string>>::error_result(
                "One or more invalid file paths");
            file_impl_->executeAsync([result = std::move(result),
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
        file_impl_->asyncRead(
            files[i], [results, errors, mutex, remaining, callback, i,
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
                            error_messages += "File " + std::to_string(j) +
                                              ": " + (*errors)[j];
                        }
                    }

                    if (error_messages.empty()) {
                        auto final_result =
                            AsyncResult<std::vector<std::string>>::
                                success_result(std::move(*results));
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

void AsyncBatchOps::asyncBatchWrite(
    std::span<const std::pair<std::string, std::string>> file_data_pairs,
    std::function<void(AsyncResult<void>)> callback) {
    if (file_data_pairs.empty()) {
        if (callback) {
            auto result =
                AsyncResult<void>::error_result("Empty file-data pairs list");
            file_impl_->executeAsync([result = std::move(result),
                                      callback = std::move(callback)]() mutable {
                callback(std::move(result));
            });
        }
        return;
    }

    auto total_files = file_data_pairs.size();
    auto completed = std::make_shared<std::atomic<size_t>>(0);
    auto errors = std::make_shared<std::vector<std::string>>(total_files);

    for (size_t i = 0; i < total_files; ++i) {
        const auto& [filename, data] = file_data_pairs[i];

        file_impl_->asyncWrite(
            filename, std::span<const char>(data.data(), data.size()),
            [i, total_files, completed, errors,
             callback](AsyncResult<void> result) {
                if (!result.success) {
                    (*errors)[i] = result.error_message;
                }

                size_t current_completed = completed->fetch_add(1) + 1;
                if (current_completed == total_files) {
                    // All operations completed
                    std::string error_messages;
                    for (size_t j = 0; j < total_files; ++j) {
                        if (!(*errors)[j].empty()) {
                            if (!error_messages.empty()) {
                                error_messages += "; ";
                            }
                            error_messages += "File " + std::to_string(j) +
                                              ": " + (*errors)[j];
                        }
                    }

                    if (error_messages.empty()) {
                        auto final_result = AsyncResult<void>::success_result();
                        callback(std::move(final_result));
                    } else {
                        auto final_result = AsyncResult<void>::error_result(
                            std::move(error_messages));
                        callback(std::move(final_result));
                    }
                }
            });
    }
}

void AsyncBatchOps::asyncBatchDelete(
    std::span<const std::string> files,
    std::function<void(AsyncResult<void>)> callback) {
    if (files.empty()) {
        if (callback) {
            auto result = AsyncResult<void>::error_result("Empty file list");
            file_impl_->executeAsync([result = std::move(result),
                                      callback = std::move(callback)]() mutable {
                callback(std::move(result));
            });
        }
        return;
    }

    auto total_files = files.size();
    auto completed = std::make_shared<std::atomic<size_t>>(0);
    auto errors = std::make_shared<std::vector<std::string>>(total_files);

    for (size_t i = 0; i < total_files; ++i) {
        file_impl_->asyncDelete(
            files[i], [i, total_files, completed, errors,
                       callback](AsyncResult<void> result) {
                if (!result.success) {
                    (*errors)[i] = result.error_message;
                }

                size_t current_completed = completed->fetch_add(1) + 1;
                if (current_completed == total_files) {
                    // All operations completed
                    std::string error_messages;
                    for (size_t j = 0; j < total_files; ++j) {
                        if (!(*errors)[j].empty()) {
                            if (!error_messages.empty()) {
                                error_messages += "; ";
                            }
                            error_messages += "File " + std::to_string(j) +
                                              ": " + (*errors)[j];
                        }
                    }

                    if (error_messages.empty()) {
                        auto final_result = AsyncResult<void>::success_result();
                        callback(std::move(final_result));
                    } else {
                        auto final_result = AsyncResult<void>::error_result(
                            std::move(error_messages));
                        callback(std::move(final_result));
                    }
                }
            });
    }
}

}  // namespace atom::io::async

#include "async_io.hpp"
#include "../core/path_utils.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <vector>

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

/**
 * @brief Validates a file path for security and format compliance
 *
 * Delegates to centralized path validation utility.
 *
 * @param path The path to validate
 * @return true if path is valid and safe, false otherwise
 */
bool AsyncFile::validatePath(std::string_view path) noexcept {
    return ::atom::io::path_utils::validatePath(path);
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

bool AsyncFile::validatePermissions(std::string_view path,
                                    bool write_access) noexcept {
    if (!validatePath(path)) {
        return false;
    }

    try {
        std::filesystem::path fs_path(path);

        // Edge case: Handle root directory access
        if (fs_path == fs_path.root_path()) {
            // Root directory should be readable but typically not writable
            return !write_access;
        }

        // Check if file/directory exists
        if (!std::filesystem::exists(fs_path)) {
            // For write operations, check if parent directory exists and is
            // writable
            if (write_access) {
                auto parent = fs_path.parent_path();

                // Edge case: Handle case where parent is empty (relative path)
                if (parent.empty()) {
                    parent = std::filesystem::current_path();
                }

                if (!std::filesystem::exists(parent)) {
                    return false;
                }

                // Edge case: Check if parent is actually a directory
                if (!std::filesystem::is_directory(parent)) {
                    return false;
                }

                return std::filesystem::is_directory(parent);
            }
            return false;
        }

        // Edge case: Handle symbolic links
        std::error_code ec;
        if (std::filesystem::is_symlink(fs_path, ec)) {
            if (ec) {
                return false;
            }
            // Check if the symlink target exists and is accessible
            auto target = std::filesystem::read_symlink(fs_path, ec);
            if (ec || !std::filesystem::exists(target, ec) || ec) {
                return false;
            }
        }

        // Check read permissions
        auto perms = std::filesystem::status(fs_path, ec).permissions();
        if (ec) {
            return false;
        }

        // Basic permission check (this is platform-dependent)
        using perms_t = std::filesystem::perms;
        bool readable = (perms & perms_t::owner_read) != perms_t::none ||
                        (perms & perms_t::group_read) != perms_t::none ||
                        (perms & perms_t::others_read) != perms_t::none;

        if (!readable) {
            return false;
        }

        // Check write permissions if required
        if (write_access) {
            bool writable = (perms & perms_t::owner_write) != perms_t::none ||
                            (perms & perms_t::group_write) != perms_t::none ||
                            (perms & perms_t::others_write) != perms_t::none;
            return writable;
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Permission validation failed: {}", e.what());
        return false;
    }
}

std::string AsyncFile::sanitizeFilename(std::string_view filename) noexcept {
    if (filename.empty()) {
        return "unnamed_file";
    }

    std::string result;
    result.reserve(filename.length());

    for (char c : filename) {
        // Replace invalid characters with underscores
        if (c == '\0' || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            result += '_';
        } else if (c < 32 || c == 127) {  // Control characters
            result += '_';
        } else {
            result += c;
        }
    }

    // Trim leading/trailing spaces and dots
    while (!result.empty() &&
           (result.front() == ' ' || result.front() == '.')) {
        result.erase(0, 1);
    }
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }

    // Ensure result is not empty
    if (result.empty()) {
        result = "unnamed_file";
    }

    // Truncate if too long
    if (result.length() > 255) {
        result = result.substr(0, 255);
    }

#ifdef _WIN32
    // Check for reserved names on Windows and append suffix if needed
    std::string upper_result = result;
    std::transform(upper_result.begin(), upper_result.end(),
                   upper_result.begin(), ::toupper);

    const std::vector<std::string> reserved_names = {
        "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
        "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
        "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

    for (const auto& reserved : reserved_names) {
        if (upper_result == reserved ||
            upper_result.starts_with(reserved + ".")) {
            result += "_file";
            break;
        }
    }
#endif

    return result;
}

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

/**
 * @brief High-performance SIMD-optimized buffer comparison
 *
 * Uses vectorized instructions (AVX2/SSE4.2) when available for optimal
 * performance. Falls back to standard library implementation on unsupported
 * platforms.
 *
 * @param buffer1 First buffer to compare
 * @param buffer2 Second buffer to compare
 * @return true if buffers are identical, false otherwise
 */
bool AsyncFile::simdBufferCompare(std::span<const char> buffer1,
                                  std::span<const char> buffer2) noexcept {
    if (buffer1.size() != buffer2.size()) {
        return false;
    }

    if (buffer1.empty()) {
        return true;
    }

    const char* data1 = buffer1.data();
    const char* data2 = buffer2.data();
    size_t size = buffer1.size();

#ifdef ATOM_HAS_AVX2
    // Process 32 bytes at a time with AVX2
    size_t i = 0;
    for (; i + 32 <= size; i += 32) {
        __m256i chunk1 =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data1 + i));
        __m256i chunk2 =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data2 + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk1, chunk2);
        int mask = _mm256_movemask_epi8(cmp);
        if (mask != 0xFFFFFFFF) {
            return false;
        }
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        if (data1[i] != data2[i]) {
            return false;
        }
    }
    return true;
#elif defined(ATOM_HAS_SSE42)
    // Process 16 bytes at a time with SSE4.2
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        __m128i chunk1 =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(data1 + i));
        __m128i chunk2 =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(data2 + i));
        __m128i cmp = _mm_cmpeq_epi8(chunk1, chunk2);
        int mask = _mm_movemask_epi8(cmp);
        if (mask != 0xFFFF) {
            return false;
        }
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        if (data1[i] != data2[i]) {
            return false;
        }
    }
    return true;
#else
    // Fallback to standard library
    return std::equal(buffer1.begin(), buffer1.end(), buffer2.begin());
#endif
}

/**
 * @brief SIMD-optimized byte search in buffer
 *
 * Efficiently searches for a specific byte using vectorized instructions.
 * Processes 32 bytes at a time with AVX2 or 16 bytes with SSE4.2.
 *
 * @param buffer Buffer to search in
 * @param target Byte value to find
 * @return Position of first occurrence or std::string::npos if not found
 */
size_t AsyncFile::simdFindByte(std::span<const char> buffer,
                               char target) noexcept {
    if (buffer.empty()) {
        return std::string::npos;
    }

    const char* data = buffer.data();
    size_t size = buffer.size();

#ifdef ATOM_HAS_AVX2
    const __m256i target_vec = _mm256_set1_epi8(target);
    size_t i = 0;

    // Process 32 bytes at a time
    for (; i + 32 <= size; i += 32) {
        __m256i chunk =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, target_vec);
        int mask = _mm256_movemask_epi8(cmp);

        if (mask != 0) {
            return i + __builtin_ctz(mask);
        }
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        if (data[i] == target) {
            return i;
        }
    }
    return std::string::npos;
#elif defined(ATOM_HAS_SSE42)
    const __m128i target_vec = _mm_set1_epi8(target);
    size_t i = 0;

    // Process 16 bytes at a time
    for (; i + 16 <= size; i += 16) {
        __m128i chunk =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i cmp = _mm_cmpeq_epi8(chunk, target_vec);
        int mask = _mm_movemask_epi8(cmp);

        if (mask != 0) {
            return i + __builtin_ctz(mask);
        }
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        if (data[i] == target) {
            return i;
        }
    }
    return std::string::npos;
#else
    // Fallback to standard library
    auto it = std::find(buffer.begin(), buffer.end(), target);
    return it != buffer.end() ? std::distance(buffer.begin(), it)
                              : std::string::npos;
#endif
}

/**
 * @brief SIMD-optimized memory initialization
 *
 * Efficiently sets all bytes in a buffer to a specific value using vectorized
 * instructions. Processes 32 bytes at a time with AVX2 or 16 bytes with SSE4.2.
 *
 * @param buffer Buffer to initialize
 * @param value Byte value to set
 */
void AsyncFile::simdMemorySet(std::span<char> buffer, char value) noexcept {
    if (buffer.empty()) {
        return;
    }

    char* data = buffer.data();
    size_t size = buffer.size();

#ifdef ATOM_HAS_AVX2
    const __m256i value_vec = _mm256_set1_epi8(value);
    size_t i = 0;

    // Process 32 bytes at a time
    for (; i + 32 <= size; i += 32) {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i), value_vec);
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        data[i] = value;
    }
#elif defined(ATOM_HAS_SSE42)
    const __m128i value_vec = _mm_set1_epi8(value);
    size_t i = 0;

    // Process 16 bytes at a time
    for (; i + 16 <= size; i += 16) {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), value_vec);
    }

    // Handle remaining bytes
    for (; i < size; ++i) {
        data[i] = value;
    }
#else
    // Fallback to standard library
    std::fill(buffer.begin(), buffer.end(), value);
#endif
}

void AsyncFile::asyncBatchWrite(
    std::span<const std::pair<std::string, std::string>> file_data_pairs,
    std::function<void(AsyncResult<void>)> callback) {
    if (file_data_pairs.empty()) {
        if (callback) {
            auto result =
                AsyncResult<void>::error_result("Empty file-data pairs list");
            executeAsync([result = std::move(result),
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

        asyncWrite(
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

void AsyncFile::asyncBatchDelete(
    std::span<const std::string> files,
    std::function<void(AsyncResult<void>)> callback) {
    if (files.empty()) {
        if (callback) {
            auto result = AsyncResult<void>::error_result("Empty file list");
            executeAsync([result = std::move(result),
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
        asyncDelete(files[i], [i, total_files, completed, errors,
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
                        error_messages +=
                            "File " + std::to_string(j) + ": " + (*errors)[j];
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

// Explicit template instantiations for common types
template void AsyncFile::executeAsync<std::function<void()>>(
    std::function<void()>&&);

#ifndef ATOM_USE_ASIO
template void AsyncFile::scheduleTimeout<std::function<void()>>(
    std::chrono::milliseconds, std::function<void()>&&);
#endif

// Template instantiations for PathString template functions
template std::string AsyncFile::toString<std::string>(std::string&&);
template std::string AsyncFile::toString<std::string_view>(std::string_view&&);
template std::string AsyncFile::toString<const char*>(const char*&&);
template std::string AsyncFile::toString<std::filesystem::path>(
    std::filesystem::path&&);

// AsyncRead instantiations
template void AsyncFile::asyncRead<std::string>(
    std::string&&, std::function<void(AsyncResult<std::string>)>);
template void AsyncFile::asyncRead<std::string_view>(
    std::string_view&&, std::function<void(AsyncResult<std::string>)>);
template void AsyncFile::asyncRead<const char*>(
    const char*&&, std::function<void(AsyncResult<std::string>)>);
template void AsyncFile::asyncRead<std::filesystem::path>(
    std::filesystem::path&&, std::function<void(AsyncResult<std::string>)>);

// AsyncWrite instantiations
template void AsyncFile::asyncWrite<std::string>(
    std::string&&, std::span<const char>,
    std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncWrite<std::string_view>(
    std::string_view&&, std::span<const char>,
    std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncWrite<const char*>(
    const char*&&, std::span<const char>,
    std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncWrite<std::filesystem::path>(
    std::filesystem::path&&, std::span<const char>,
    std::function<void(AsyncResult<void>)>);

// AsyncDelete instantiations
template void AsyncFile::asyncDelete<std::string>(
    std::string&&, std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncDelete<std::string_view>(
    std::string_view&&, std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncDelete<const char*>(
    const char*&&, std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncDelete<std::filesystem::path>(
    std::filesystem::path&&, std::function<void(AsyncResult<void>)>);

// AsyncStat instantiations
template void AsyncFile::asyncStat<std::string>(
    std::string&&,
    std::function<void(AsyncResult<std::filesystem::file_status>)>);
template void AsyncFile::asyncStat<std::string_view>(
    std::string_view&&,
    std::function<void(AsyncResult<std::filesystem::file_status>)>);
template void AsyncFile::asyncStat<const char*>(
    const char*&&,
    std::function<void(AsyncResult<std::filesystem::file_status>)>);
template void AsyncFile::asyncStat<std::filesystem::path>(
    std::filesystem::path&&,
    std::function<void(AsyncResult<std::filesystem::file_status>)>);

// AsyncChangePermissions instantiations
template void AsyncFile::asyncChangePermissions<std::string>(
    std::string&&, std::filesystem::perms,
    std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncChangePermissions<std::string_view>(
    std::string_view&&, std::filesystem::perms,
    std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncChangePermissions<const char*>(
    const char*&&, std::filesystem::perms,
    std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncChangePermissions<std::filesystem::path>(
    std::filesystem::path&&, std::filesystem::perms,
    std::function<void(AsyncResult<void>)>);

// AsyncExists instantiations
template void AsyncFile::asyncExists<std::string>(
    std::string&&, std::function<void(AsyncResult<bool>)>);
template void AsyncFile::asyncExists<std::string_view>(
    std::string_view&&, std::function<void(AsyncResult<bool>)>);
template void AsyncFile::asyncExists<const char*>(
    const char*&&, std::function<void(AsyncResult<bool>)>);
template void AsyncFile::asyncExists<std::filesystem::path>(
    std::filesystem::path&&, std::function<void(AsyncResult<bool>)>);

// Directory operations instantiations
template void AsyncFile::asyncCreateDirectory<std::string>(
    std::string&&, std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncCreateDirectory<std::string_view>(
    std::string_view&&, std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncCreateDirectory<const char*>(
    const char*&&, std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncCreateDirectory<std::filesystem::path>(
    std::filesystem::path&&, std::function<void(AsyncResult<void>)>);

template void AsyncFile::asyncRemoveDirectory<std::string>(
    std::string&&, std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncRemoveDirectory<std::string_view>(
    std::string_view&&, std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncRemoveDirectory<const char*>(
    const char*&&, std::function<void(AsyncResult<void>)>);
template void AsyncFile::asyncRemoveDirectory<std::filesystem::path>(
    std::filesystem::path&&, std::function<void(AsyncResult<void>)>);

template void AsyncFile::asyncListDirectory<std::string>(
    std::string&&,
    std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>);
template void AsyncFile::asyncListDirectory<std::string_view>(
    std::string_view&&,
    std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>);
template void AsyncFile::asyncListDirectory<const char*>(
    const char*&&,
    std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>);
template void AsyncFile::asyncListDirectory<std::filesystem::path>(
    std::filesystem::path&&,
    std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>);

}  // namespace atom::async::io

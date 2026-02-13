#include "async_file.hpp"
#include "../core/path_utils.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

namespace atom::io::async {

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
    return ::atom::io::detail::validatePath(path);
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

}  // namespace atom::io::async

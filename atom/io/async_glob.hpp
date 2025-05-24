#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <coroutine>
#include <filesystem>
#include <future>
#include <memory>
#include <regex>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>
#include <asio.hpp>
#include "atom/error/exception.hpp"

namespace atom::io {

namespace fs = std::filesystem;

// Concept for validating callback types
template <typename T>
concept GlobCallbackInvocable = std::invocable<T, std::vector<fs::path>>;

/**
 * @brief Class for performing asynchronous file globbing operations.
 */
class AsyncGlob {
public:
    // Coroutine task type for glob operations
    template <typename T>
    class [[nodiscard]] Task {
    public:
        struct Promise {
            T result;

            Task<T> get_return_object() {
                return Task{
                    std::coroutine_handle<Promise>::from_promise(*this)};
            }

            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }

            void return_value(T value) noexcept { result = std::move(value); }

            void unhandled_exception() { std::terminate(); }
        };

        using promise_type = Promise;

        Task(std::coroutine_handle<Promise> h) : handle_(h) {}
        ~Task() {
            if (handle_)
                handle_.destroy();
        }

        Task(Task&& other) noexcept
            : handle_(std::exchange(other.handle_, {})) {}
        Task& operator=(Task&& other) noexcept {
            if (this != &other) {
                if (handle_)
                    handle_.destroy();
                handle_ = std::exchange(other.handle_, {});
            }
            return *this;
        }

        // Disable copy
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        T get_result() const& { return handle_.promise().result; }

        T&& get_result() && { return std::move(handle_.promise().result); }

    private:
        std::coroutine_handle<Promise> handle_;
    };

    /**
     * @brief Constructs an AsyncGlob object.
     * @param io_context The ASIO I/O context.
     */
    explicit AsyncGlob(asio::io_context& io_context) noexcept;

    /**
     * @brief Performs a glob operation to match files.
     * @param pathname The pattern to match files.
     * @param callback The callback function to call with the matched files.
     * @param recursive Whether to search directories recursively.
     * @param dironly Whether to match directories only.
     * @throws atom::error::Exception if path expansion fails
     */
    template <GlobCallbackInvocable Callback>
    void glob(std::string_view pathname, Callback&& callback,
              bool recursive = false, bool dironly = false);

    /**
     * @brief Performs a glob operation as a coroutine task.
     * @param pathname The pattern to match files.
     * @param recursive Whether to search directories recursively.
     * @param dironly Whether to match directories only.
     * @return A coroutine task that will resolve to a vector of paths.
     * @throws atom::error::Exception if path expansion fails
     */
    [[nodiscard]] Task<std::vector<fs::path>> glob_async(
        std::string_view pathname, bool recursive = false,
        bool dironly = false);

    /**
     * @brief Performs a glob operation synchronously.
     * @param pathname The pattern to match files.
     * @param recursive Whether to search directories recursively.
     * @param dironly Whether to match directories only.
     * @return A vector of matched paths.
     * @throws atom::error::Exception if path expansion fails
     */
    [[nodiscard]] std::vector<fs::path> glob_sync(std::string_view pathname,
                                                  bool recursive = false,
                                                  bool dironly = false);

private:
    /**
     * @brief Translates a glob pattern to a regular expression.
     * @param pattern The glob pattern.
     * @return The translated regular expression.
     * @throws std::regex_error if the pattern cannot be compiled
     */
    [[nodiscard]] auto translate(std::string_view pattern) const -> std::string;

    /**
     * @brief Compiles a glob pattern into a regular expression.
     * @param pattern The glob pattern.
     * @return The compiled regular expression.
     * @throws std::regex_error if the pattern cannot be compiled
     */
    [[nodiscard]] auto compilePattern(std::string_view pattern) const
        -> std::regex;

    /**
     * @brief Matches a file name against a glob pattern.
     * @param name The file name.
     * @param pattern The glob pattern.
     * @return True if the file name matches the pattern, false otherwise.
     */
    [[nodiscard]] auto fnmatch(const fs::path& name,
                               std::string_view pattern) const noexcept -> bool;

    /**
     * @brief Filters a list of file names against a glob pattern.
     * @param names The list of file names.
     * @param pattern The glob pattern.
     * @return The filtered list of file names.
     */
    [[nodiscard]] auto filter(std::span<const fs::path> names,
                              std::string_view pattern) const
        -> std::vector<fs::path>;

    /**
     * @brief Expands a tilde in a file path to the home directory.
     * @param path The file path.
     * @return The expanded file path.
     * @throws atom::error::Exception if home directory expansion fails
     */
    [[nodiscard]] auto expandTilde(const fs::path& path) const -> fs::path;

    /**
     * @brief Checks if a pathname contains glob magic characters.
     * @param pathname The pathname to check.
     * @return True if the pathname contains magic characters, false otherwise.
     */
    [[nodiscard]] static auto hasMagic(std::string_view pathname) noexcept
        -> bool;

    /**
     * @brief Checks if a pathname is hidden.
     * @param pathname The pathname to check.
     * @return True if the pathname is hidden, false otherwise.
     */
    [[nodiscard]] static auto isHidden(std::string_view pathname) noexcept
        -> bool;

    /**
     * @brief Checks if a glob pattern is recursive.
     * @param pattern The glob pattern.
     * @return True if the pattern is recursive, false otherwise.
     */
    [[nodiscard]] static auto isRecursive(std::string_view pattern) noexcept
        -> bool;

    /**
     * @brief Iterates over a directory and calls a callback with the file
     * names.
     * @param dirname The directory to iterate.
     * @param dironly Whether to match directories only.
     * @param callback The callback function to call with the file names.
     */
    template <GlobCallbackInvocable Callback>
    void iterDirectory(const fs::path& dirname, bool dironly,
                       Callback&& callback);

    /**
     * @brief Recursively lists files in a directory and calls a callback with
     * the file names.
     * @param dirname The directory to list.
     * @param dironly Whether to match directories only.
     * @param callback The callback function to call with the file names.
     */
    void rlistdir(const fs::path& dirname, bool dironly,
                  std::function<void(std::vector<fs::path>)> callback,
                  int depth);

    /**
     * @brief Performs a glob operation in a directory with recursive pattern.
     * @param dirname The directory to search.
     * @param pattern The glob pattern.
     * @param dironly Whether to match directories only.
     * @param callback The callback function to call with the matched files.
     */
    template <GlobCallbackInvocable Callback>
    void glob2(const fs::path& dirname, std::string_view pattern, bool dironly,
               Callback&& callback);

    /**
     * @brief Performs a glob operation in a directory with regular pattern.
     * @param dirname The directory to search.
     * @param pattern The glob pattern.
     * @param dironly Whether to match directories only.
     * @param callback The callback function to call with the matched files.
     */
    template <GlobCallbackInvocable Callback>
    void glob1(const fs::path& dirname, std::string_view pattern, bool dironly,
               Callback&& callback);

    /**
     * @brief Performs a glob operation in a directory with literal name.
     * @param dirname The directory to search.
     * @param basename The base name to match.
     * @param dironly Whether to match directories only.
     * @param callback The callback function to call with the matched files.
     */
    template <GlobCallbackInvocable Callback>
    void glob0(const fs::path& dirname, const fs::path& basename, bool dironly,
               Callback&& callback);

    // Thread pool for parallel processing
    std::unique_ptr<std::vector<std::thread>> thread_pool_;

    // Cache for compiled regex patterns
    mutable std::unordered_map<std::string, std::shared_ptr<std::regex>>
        pattern_cache_;
    mutable std::mutex pattern_cache_mutex_;

    asio::io_context& io_context_;  ///< The ASIO I/O context.
};

}  // namespace atom::io

#pragma once

namespace atom::io {

template <GlobCallbackInvocable Callback>
void AsyncGlob::iterDirectory(const fs::path& dirname, bool dironly,
                              Callback&& callback) {
    spdlog::info(
        "AsyncGlob::iterDirectory called with dirname: {}, dironly: {}",
        dirname.string(), dironly);

    io_context_.post([dirname, dironly,
                      callback = std::forward<Callback>(callback)]() mutable {
        std::vector<fs::path> result;
        auto currentDirectory = dirname;
        if (currentDirectory.empty()) {
            currentDirectory = fs::current_path();
        }

        // Validate the directory exists before iterating
        if (!fs::exists(currentDirectory)) {
            spdlog::warn("Directory does not exist: {}",
                         currentDirectory.string());
            callback({});
            return;
        }

        try {
            // Iterate through directory safely, handling any errors
            for (const auto& entry : fs::directory_iterator(
                     currentDirectory,
                     fs::directory_options::follow_directory_symlink |
                         fs::directory_options::skip_permission_denied)) {
                if (!dironly || entry.is_directory()) {
                    if (dirname.is_absolute()) {
                        result.push_back(entry.path());
                    } else {
                        result.push_back(fs::relative(entry.path()));
                    }
                }
            }
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Filesystem error in iterDirectory: {}", e.what());
        } catch (const std::exception& e) {
            spdlog::error("Exception in iterDirectory: {}", e.what());
        }

        callback(std::move(result));
    });
}

template <GlobCallbackInvocable Callback>
void AsyncGlob::glob2(const fs::path& dirname, std::string_view pattern,
                      bool dironly, Callback&& callback) {
    spdlog::info(
        "AsyncGlob::glob2 called with dirname: {}, pattern: {}, dironly: {}",
        dirname.string(), pattern, dironly);

    assert(isRecursive(pattern));
    this->rlistdir(dirname, dironly,
                   std::function<void(std::vector<fs::path>)>(
                       std::forward<Callback>(callback)),
                   0);
}
template <GlobCallbackInvocable Callback>
void AsyncGlob::glob1(const fs::path& dirname, std::string_view pattern,
                      bool dironly, Callback&& callback) {
    spdlog::info(
        "AsyncGlob::glob1 called with dirname: {}, pattern: {}, dironly: {}",
        dirname.string(), pattern, dironly);

    iterDirectory(
        dirname, dironly,
        [this, pattern = std::string(pattern),
         callback = std::forward<Callback>(callback)](
            std::vector<fs::path> names) mutable {
            std::vector<fs::path> filteredNames;
            filteredNames.reserve(names.size());

            // Extract the base names for matching
            std::vector<fs::path> baseNames;
            baseNames.reserve(names.size());

            for (const auto& name : names) {
                baseNames.push_back(name.filename());
            }

            // Filter names based on pattern
            auto matchedNames = filter(baseNames, pattern);

            // Convert back to full paths
            for (const auto& name : names) {
                if (std::find_if(matchedNames.begin(), matchedNames.end(),
                                 [&name](const fs::path& match) {
                                     return match == name.filename();
                                 }) != matchedNames.end()) {
                    filteredNames.push_back(name);
                }
            }

            callback(std::move(filteredNames));
        });
}

template <GlobCallbackInvocable Callback>
void AsyncGlob::glob0(const fs::path& dirname, const fs::path& basename,
                      bool dironly, Callback&& callback) {
    spdlog::info(
        "AsyncGlob::glob0 called with dirname: {}, basename: {}, dironly: {}",
        dirname.string(), basename.string(), dironly);

    fs::path path;
    if (dirname.empty()) {
        path = basename;
    } else {
        path = dirname / basename;
    }

    io_context_.post([path = std::move(path), dironly,
                      callback = std::forward<Callback>(callback)]() mutable {
        std::vector<fs::path> result;

        try {
            if (fs::exists(path) && (!dironly || fs::is_directory(path))) {
                result.push_back(path);
            }
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Filesystem error in glob0: {}", e.what());
        }

        callback(std::move(result));
    });
}

template <GlobCallbackInvocable Callback>
void AsyncGlob::glob(std::string_view pathname, Callback&& callback,
                     bool recursive, bool dironly) {
    spdlog::info(
        "AsyncGlob::glob called with pathname: {}, recursive: {}, dironly: {}",
        pathname, recursive, dironly);

    try {
        std::string pathnameStr(pathname);
        fs::path path(pathnameStr);
        path = expandTilde(path);

        if (recursive && !hasMagic(pathnameStr)) {
            // No magic, but we want to match recursively
            this->rlistdir(path, dironly, std::forward<Callback>(callback), 0);
            return;
        }

        // Split path into dirname and basename
        fs::path dirname, basename;

        if (path.has_filename()) {
            basename = path.filename();
            dirname = path.parent_path();
        } else {
            basename = path;
        }

        if (basename.empty()) {
            iterDirectory(dirname, dironly, std::forward<Callback>(callback));
            return;
        }

        // Check if basename has magic characters
        std::string basenameStr = basename.string();
        if (!hasMagic(basenameStr)) {
            // No magic in basename
            glob0(dirname, basename, dironly, std::forward<Callback>(callback));
            return;
        }

        // Handle recursive pattern "**"
        if (isRecursive(basenameStr)) {
            // Recursive pattern
            glob2(dirname, basenameStr, dironly,
                  std::forward<Callback>(callback));
            return;
        }

        // Handle regular pattern matching
        glob1(dirname, basenameStr, dironly, std::forward<Callback>(callback));
    } catch (const std::exception& e) {
        spdlog::error("Exception in glob: {}", e.what());
        callback({});
    }
}

inline AsyncGlob::Task<std::vector<fs::path>> AsyncGlob::glob_async(
    std::string_view pathname, bool recursive, bool dironly) {
    spdlog::info(
        "AsyncGlob::glob_async called with pathname: {}, recursive: {}, "
        "dironly: {}",
        pathname, recursive, dironly);

    std::vector<fs::path> result;

    try {
        std::promise<std::vector<fs::path>> promise;
        auto future = promise.get_future();

        glob(
            pathname,
            [&promise](std::vector<fs::path> paths) {
                promise.set_value(std::move(paths));
            },
            recursive, dironly);

        result = future.get();
    } catch (const std::exception& e) {
        spdlog::error("Exception in glob_async: {}", e.what());
        THROW_EXCEPTION("Error in glob_async: {}", e.what());
    }

    co_return result;
}

}  // namespace atom::io

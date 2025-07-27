#include "async_glob.hpp"
#include "glob.hpp"

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"

#include <future>
#include <mutex>
#include <string_view>
#include <thread>

#ifdef __SSE4_2__
#include <immintrin.h>
#endif

#if ATOM_ENABLE_ABSL
#include <absl/strings/match.h>
#include <absl/strings/str_replace.h>
#endif

namespace atom::io {

AsyncGlob::AsyncGlob(asio::io_context& io_context, const GlobConfig& config) noexcept
    : io_context_(io_context), config_(config) {
    if (config_.enable_statistics) {
        spdlog::debug("AsyncGlob constructor called with {} threads", config_.max_thread_count);
    }

    // Initialize thread pool with configured thread count
    if (config_.max_thread_count > 0) {
        thread_pool_ = std::make_unique<asio::thread_pool>(config_.max_thread_count);
    }

    // Initialize statistics
    stats_.start_time = std::chrono::steady_clock::now();
}

auto AsyncGlob::translate(std::string_view pattern) const -> std::string {
    if (config_.enable_statistics) {
        spdlog::debug("AsyncGlob::translate called with pattern: {}", pattern);
    }

    if (pattern.empty()) {
        return "(.*)";
    }

    std::size_t index = 0;
    const std::size_t patternSize = pattern.size();
    std::string resultString;
    resultString.reserve(patternSize * 2);

    try {
        while (index < patternSize) {
            auto currentChar = pattern[index++];
            switch (currentChar) {
                case '*':
                    resultString += ".*";
                    break;
                case '?':
                    resultString += ".";
                    break;
                case '[': {
                    auto innerIndex = index;
                    if (innerIndex < patternSize &&
                        pattern[innerIndex] == '!') {
                        innerIndex++;
                    }
                    if (innerIndex < patternSize &&
                        pattern[innerIndex] == ']') {
                        innerIndex++;
                    }

                    while (innerIndex < patternSize &&
                           pattern[innerIndex] != ']') {
                        innerIndex++;
                    }

                    if (innerIndex >= patternSize) {
                        resultString += "\\[";
                    } else {
                        auto stuff = std::string(
                            pattern.substr(index, innerIndex - index));

#if ATOM_ENABLE_ABSL
                        if (!absl::StrContains(stuff, "--")) {
#else
                        if (stuff.find("--") == std::string::npos) {
#endif
                            std::string tempStuff = stuff;
                            while (stringReplace(tempStuff, std::string{"\\"},
                                                 std::string{R"(\\)"})) {
                            }
                            stuff = tempStuff;
                        } else {
                            std::vector<std::string> chunks;
                            std::size_t chunkIndex = 0;
                            if (pattern[index] == '!') {
                                chunkIndex = index + 2;
                            } else {
                                chunkIndex = index + 1;
                            }

                            while (true) {
                                auto pos = pattern.find('-', chunkIndex);
                                if (pos == std::string_view::npos ||
                                    pos >= innerIndex) {
                                    break;
                                }
                                chunks.emplace_back(
                                    pattern.substr(index, pos - index));
                                index = pos + 1;
                                chunkIndex = pos + 3;
                            }

                            chunks.emplace_back(
                                pattern.substr(index, innerIndex - index));
                            bool first = true;
                            for (auto& chunk : chunks) {
                                while (stringReplace(chunk, std::string{"\\"},
                                                     std::string{R"(\\)"})) {
                                }
                                while (stringReplace(chunk, std::string{"-"},
                                                     std::string{R"(\-)"})) {
                                };
                                if (first) {
                                    stuff += chunk;
                                    first = false;
                                } else {
                                    stuff += "-" + chunk;
                                }
                            }
                        }

                        std::string result;
                        result.reserve(stuff.size() * 2);
                        std::regex_replace(std::back_inserter(result),
                                           stuff.begin(), stuff.end(),
                                           std::regex(std::string{R"([&~|])"}),
                                           std::string{R"(\\\1)"});
                        stuff = result;
                        index = innerIndex + 1;
                        if (!stuff.empty() && stuff[0] == '!') {
                            stuff = "^" +
                                    std::string(stuff.begin() + 1, stuff.end());
                        } else if (!stuff.empty() &&
                                   (stuff[0] == '^' || stuff[0] == '[')) {
                            stuff.insert(0, "\\\\");
                        }
                        resultString += "[" + stuff + "]";
                    }
                    break;
                }
                default: {
                    static const std::string_view specialCharacters =
                        "()[]{}?*+-|^$\\.&~# \t\n\r\v\f";

#ifdef __SSE4_2__
                    if (specialCharacters.size() >= 16) {
                        __m128i needle = _mm_set1_epi8(currentChar);
                        __m128i haystack =
                            _mm_loadu_si128(reinterpret_cast<const __m128i*>(
                                specialCharacters.data()));
                        int mask = _mm_cmpestrm(
                            haystack,
                            static_cast<int>(specialCharacters.size()), needle,
                            1, _SIDD_CMP_EQUAL_ANY);
                        if (mask) {
                            resultString += '\\';
                        }
                    } else {
                        if (specialCharacters.find(currentChar) !=
                            std::string_view::npos) {
                            resultString += '\\';
                        }
                    }
#else
                    if (specialCharacters.find(currentChar) !=
                        std::string_view::npos) {
                        resultString += '\\';
                    }
#endif

                    resultString += currentChar;
                    break;
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in translate: {}", e.what());
        throw;
    }

    if (config_.enable_statistics) {
        spdlog::debug("Translated pattern: {}", resultString);
    }
    return std::string{"(("} + resultString + std::string{R"()|[\r\n])$)"};
}

auto AsyncGlob::compilePattern(std::string_view pattern) const -> std::regex {
    if (config_.enable_statistics) {
        spdlog::debug("AsyncGlob::compilePattern called with pattern: {}", pattern);
    }

    std::string pattern_str(pattern);

    {
        std::lock_guard<std::mutex> lock(pattern_cache_mutex_);
        auto it = pattern_cache_.find(pattern_str);
        if (it != pattern_cache_.end()) {
            // Update access time for LRU
            cache_access_times_[pattern_str] = std::chrono::steady_clock::now();
            if (config_.enable_statistics) {
                ++stats_.cache_hits;
            }
            return *it->second;
        }
    }

    try {
        auto regex_ptr = std::make_shared<std::regex>(
            translate(pattern), std::regex::ECMAScript | std::regex::optimize);

        {
            std::lock_guard<std::mutex> lock(pattern_cache_mutex_);
            pattern_cache_[pattern_str] = regex_ptr;
            cache_access_times_[pattern_str] = std::chrono::steady_clock::now();

            if (config_.enable_statistics) {
                ++stats_.cache_misses;
            }

            // Cleanup cache if it's getting too large
            if (pattern_cache_.size() > config_.pattern_cache_size) {
                // Remove this from the critical section by posting cleanup
                io_context_.post([this]() {
                    const_cast<AsyncGlob*>(this)->cleanupPatternCache();
                });
            }
        }

        return *regex_ptr;
    } catch (const std::regex_error& e) {
        spdlog::error("Regex compilation error for pattern '{}': {}", pattern, e.what());
        throw;
    }
}

auto AsyncGlob::fnmatch(const fs::path& name,
                        std::string_view pattern) const noexcept -> bool {
    try {
        if (config_.enable_statistics) {
            spdlog::debug("AsyncGlob::fnmatch called with name: {}, pattern: {}",
                         name.string(), pattern);
        }

        // Try fast matching first if pattern can be optimized
        if (config_.enable_pattern_optimization && canOptimizePattern(pattern)) {
            bool result = fastMatch(name.string(), pattern);
            if (config_.enable_statistics) {
                spdlog::debug("AsyncGlob::fnmatch (fast) returning: {}", result);
            }
            return result;
        }

        bool result = std::regex_match(name.string(), compilePattern(pattern));
        if (config_.enable_statistics) {
            spdlog::debug("AsyncGlob::fnmatch returning: {}", result);
        }
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Exception in fnmatch: {}", e.what());
        return false;
    }
}

auto AsyncGlob::filter(std::span<const fs::path> names,
                       std::string_view pattern) const
    -> std::vector<fs::path> {
    spdlog::info("AsyncGlob::filter called with pattern: {}", pattern);

    try {
        auto compiled_pattern = compilePattern(pattern);
        std::vector<fs::path> result;
        result.reserve(names.size() / 2);

        if (thread_pool_ && config_.max_thread_count > 1 && names.size() > config_.parallel_threshold) {
            const size_t chunk_size = (names.size() + config_.max_thread_count - 1) / config_.max_thread_count;
            std::vector<std::future<std::vector<fs::path>>> futures;

            for (size_t i = 0; i < names.size(); i += chunk_size) {
                const size_t end = std::min(i + chunk_size, names.size());
                futures.push_back(std::async(std::launch::async, [&, i, end]() {
                    std::vector<fs::path> chunk_result;
                    for (size_t j = i; j < end; ++j) {
                        if (std::regex_match(names[j].string(), compiled_pattern)) {
                            chunk_result.push_back(names[j]);
                        }
                    }
                    return chunk_result;
                }));
            }

            for (auto& future : futures) {
                auto chunk_result = future.get();
                result.insert(result.end(),
                              std::make_move_iterator(chunk_result.begin()),
                              std::make_move_iterator(chunk_result.end()));
            }
        } else {
            for (const auto& name : names) {
                if (std::regex_match(name.string(), compiled_pattern)) {
                    result.push_back(name);
                }
            }
        }

        spdlog::info("AsyncGlob::filter returning {} paths", result.size());
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Exception in filter: {}", e.what());
        throw;
    }
}

auto AsyncGlob::expandTilde(const fs::path& path) const -> fs::path {
    spdlog::info("AsyncGlob::expandTilde called with path: {}", path.string());

    if (path.empty()) {
        return path;
    }

    try {
        std::string pathStr = path.string();
        if (pathStr[0] == '~') {
#ifdef _WIN32
            const char* homeVariable = "USERNAME";
#else
            const char* homeVariable = "USER";
#endif

            const char* home = std::getenv(homeVariable);
            if (home == nullptr) {
                spdlog::error(
                    "Unable to expand ~ - HOME environment variable not set");
                THROW_INVALID_ARGUMENT(
                    "Unable to expand ~ - HOME environment variable not set");
            }

            pathStr = std::string(home) + pathStr.substr(1);
            fs::path expandedPath(pathStr);
            spdlog::info("Expanded path: {}", expandedPath.string());
            return expandedPath;
        }
        return path;
    } catch (const std::exception& e) {
        spdlog::error("Exception in expandTilde: {}", e.what());
        throw;
    }
}

auto AsyncGlob::hasMagic(std::string_view pathname) noexcept -> bool {
    spdlog::info("AsyncGlob::hasMagic called with pathname: {}", pathname);

    bool result = pathname.find_first_of("*?[") != std::string_view::npos;

    spdlog::info("AsyncGlob::hasMagic returning: {}", result);
    return result;
}

auto AsyncGlob::isHidden(std::string_view pathname) noexcept -> bool {
    spdlog::info("AsyncGlob::isHidden called with pathname: {}", pathname);

    if (pathname.empty()) {
        return false;
    }

    size_t lastSlash = pathname.rfind('/');
    if (lastSlash == std::string_view::npos) {
        bool result =
            pathname[0] == '.' && pathname.size() > 1 && pathname[1] != '.';
        spdlog::info("AsyncGlob::isHidden returning: {}", result);
        return result;
    } else {
        size_t filenameStart = lastSlash + 1;
        if (filenameStart >= pathname.size()) {
            return false;
        }
        bool result = pathname[filenameStart] == '.' &&
                      (pathname.size() > filenameStart + 1) &&
                      pathname[filenameStart + 1] != '.';
        spdlog::info("AsyncGlob::isHidden returning: {}", result);
        return result;
    }
}

auto AsyncGlob::isRecursive(std::string_view pattern) noexcept -> bool {
    spdlog::info("AsyncGlob::isRecursive called with pattern: {}", pattern);
    bool result = pattern == "**";
    spdlog::info("AsyncGlob::isRecursive returning: {}", result);
    return result;
}

std::vector<fs::path> AsyncGlob::glob_sync(std::string_view pathname,
                                           bool recursive, bool dironly) {
    spdlog::info("AsyncGlob::glob_sync called with pathname: {}", pathname);

    std::promise<std::vector<fs::path>> promise;
    auto future = promise.get_future();

    glob(
        pathname,
        [&promise](std::vector<fs::path> result) {
            promise.set_value(std::move(result));
        },
        recursive, dironly);

    return future.get();
}

void AsyncGlob::rlistdir(const fs::path& dirname, bool dironly,
                         std::function<void(std::vector<fs::path>)> callback,
                         int depth) {
    spdlog::info(
        "AsyncGlob::rlistdir called with dirname: {}, dironly: {}, depth: {}",
        dirname.string(), dironly, depth);

    if (depth > 100) {
        spdlog::warn("Reached maximum recursion depth at {}", dirname.string());
        callback({});
        return;
    }

    if (!fs::is_directory(dirname)) {
        spdlog::info("Path is not a directory: {}", dirname.string());
        callback({});
        return;
    }

    iterDirectory(
        dirname, dironly,
        [this, dirname, dironly, depth,
         callback](std::vector<fs::path> names) mutable {
            std::vector<fs::path> result;
            result.reserve(names.size() * 2);

            std::vector<std::future<std::vector<fs::path>>> futures;

            for (auto& name : names) {
                if (!isHidden(name.string())) {
                    result.push_back(name);

                    if (fs::is_directory(name)) {
                        if (names.size() > 10 && thread_pool_ &&
                            config_.max_thread_count > 1) {
                            futures.push_back(std::async(
                                std::launch::async,
                                [this, name, dironly, depth]() {
                                    std::vector<fs::path> subdirResults;
                                    auto promise = std::make_shared<
                                        std::promise<std::vector<fs::path>>>();
                                    auto future = promise->get_future();

                                    this->rlistdir(
                                        name, dironly,
                                        [promise](
                                            std::vector<fs::path> results) {
                                            promise->set_value(
                                                std::move(results));
                                        },
                                        depth + 1);

                                    return future.get();
                                }));
                        } else {
                            this->rlistdir(
                                name, dironly,
                                [&result](std::vector<fs::path> subNames) {
                                    result.insert(result.end(),
                                                  std::make_move_iterator(
                                                      subNames.begin()),
                                                  std::make_move_iterator(
                                                      subNames.end()));
                                },
                                depth + 1);
                        }
                    }
                }
            }

            for (auto& future : futures) {
                auto subResults = future.get();
                result.insert(result.end(),
                              std::make_move_iterator(subResults.begin()),
                              std::make_move_iterator(subResults.end()));
            }

            callback(std::move(result));
        });
}

void AsyncGlob::glob_with_progress(std::string_view pathname,
                                  ProgressCallback progress_callback,
                                  CompletionCallback completion_callback,
                                  bool recursive, bool dironly) {
    progress_callback_ = std::move(progress_callback);
    completion_callback_ = std::move(completion_callback);

    if (config_.enable_statistics) {
        stats_.start_time = std::chrono::steady_clock::now();
    }

    glob(pathname, [this](std::vector<fs::path> results) {
        if (config_.enable_statistics) {
            stats_.end_time = std::chrono::steady_clock::now();
            stats_.updateProcessingTime();
            stats_.matches_found = results.size();
        }

        if (completion_callback_) {
            completion_callback_({}, results, stats_);
        }
    }, recursive, dironly);
}

void AsyncGlob::cancel_all() {
    cancelled_.store(true, std::memory_order_release);
    if (config_.enable_statistics) {
        spdlog::debug("All glob operations cancelled");
    }
}

const AsyncGlobStats& AsyncGlob::getStats() const noexcept {
    return stats_;
}

void AsyncGlob::updateConfig(const GlobConfig& config) {
    config_ = config;

    // Recreate thread pool if thread count changed
    if (config_.max_thread_count > 0) {
        thread_pool_ = std::make_unique<asio::thread_pool>(config_.max_thread_count);
    } else {
        thread_pool_.reset();
    }
}

std::string AsyncGlob::optimizePattern(std::string_view pattern) const {
    // Simple optimizations for common patterns
    if (pattern == "*") {
        return ".*";
    } else if (pattern.find('*') == std::string::npos &&
               pattern.find('?') == std::string::npos &&
               pattern.find('[') == std::string::npos) {
        // Literal pattern - no regex needed
        return std::string(pattern);
    }

    return "";  // No optimization possible
}

bool AsyncGlob::canOptimizePattern(std::string_view pattern) const noexcept {
    // Check if pattern can be optimized for fast matching
    return pattern.find('[') == std::string::npos &&  // No character classes
           pattern.find('\\') == std::string::npos &&  // No escapes
           std::count(pattern.begin(), pattern.end(), '*') <= 1 &&  // At most one wildcard
           std::count(pattern.begin(), pattern.end(), '?') <= 3;    // At most three single chars
}

bool AsyncGlob::fastMatch(std::string_view name, std::string_view pattern) const noexcept {
    // Fast matching for simple patterns without regex
    if (pattern == "*") {
        return true;
    }

    if (pattern.find('*') == std::string::npos && pattern.find('?') == std::string::npos) {
        // Literal match
        if (config_.case_sensitive) {
            return name == pattern;
        } else {
            return std::equal(name.begin(), name.end(), pattern.begin(), pattern.end(),
                             [](char a, char b) { return std::tolower(a) == std::tolower(b); });
        }
    }

    // Simple wildcard matching (basic implementation)
    // For more complex patterns, fall back to regex
    return false;
}

void AsyncGlob::updateProgress(std::size_t processed, std::size_t total) {
    processed_items_.store(processed, std::memory_order_release);
    total_items_.store(total, std::memory_order_release);

    if (progress_callback_ && config_.enable_progress_reporting) {
        double percentage = total > 0 ? (static_cast<double>(processed) / total * 100.0) : 0.0;
        progress_callback_(processed, total, percentage);
    }
}

void AsyncGlob::cleanupPatternCache() {
    std::lock_guard<std::mutex> lock(pattern_cache_mutex_);

    if (pattern_cache_.size() <= config_.pattern_cache_size) {
        return;
    }

    // Simple LRU eviction - remove oldest entries
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::minutes(10);  // Remove entries older than 10 minutes

    for (auto it = cache_access_times_.begin(); it != cache_access_times_.end();) {
        if (it->second < cutoff) {
            pattern_cache_.erase(it->first);
            it = cache_access_times_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace atom::io

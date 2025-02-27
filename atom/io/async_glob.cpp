#include "async_glob.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/utils/string.hpp"

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

AsyncGlob::AsyncGlob(asio::io_context& io_context) noexcept
    : io_context_(io_context) {
    LOG_F(INFO, "AsyncGlob constructor called");

    // Initialize thread pool with hardware concurrency
    const auto thread_count = std::max(1u, std::thread::hardware_concurrency());
    thread_pool_ = std::make_unique<std::vector<std::thread>>(thread_count);
}

auto AsyncGlob::translate(std::string_view pattern) const -> std::string {
    LOG_F(INFO, "AsyncGlob::translate called with pattern: {}", pattern);

    if (pattern.empty()) {
        return "(.*)";
    }

    std::size_t index = 0;
    const std::size_t patternSize = pattern.size();
    std::string resultString;
    resultString.reserve(patternSize * 2);  // Preallocation for efficiency

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
                            stuff = atom::utils::replaceString(
                                stuff, std::string{"\\"}, std::string{R"(\\)"});
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
                                chunk = atom::utils::replaceString(
                                    chunk, std::string{"\\"},
                                    std::string{R"(\\)"});
                                chunk = atom::utils::replaceString(
                                    chunk, std::string{"-"},
                                    std::string{R"(\-)"});
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

// Use SSE4.2 for fast character search if available
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
        LOG_F(ERROR, "Exception in translate: {}", e.what());
        throw;
    }

    LOG_F(INFO, "Translated pattern: {}", resultString);
    return std::string{"(("} + resultString + std::string{R"()|[\r\n])$)"};
}

auto AsyncGlob::compilePattern(std::string_view pattern) const -> std::regex {
    LOG_F(INFO, "AsyncGlob::compilePattern called with pattern: {}", pattern);

    // Check pattern cache first
    {
        std::string pattern_str(pattern);
        std::lock_guard<std::mutex> lock(pattern_cache_mutex_);
        auto it = pattern_cache_.find(pattern_str);
        if (it != pattern_cache_.end()) {
            return *it->second;
        }
    }

    try {
        auto regex_ptr = std::make_shared<std::regex>(
            translate(pattern), std::regex::ECMAScript | std::regex::optimize);

        // Cache the compiled pattern
        {
            std::string pattern_str(pattern);
            std::lock_guard<std::mutex> lock(pattern_cache_mutex_);
            pattern_cache_[pattern_str] = regex_ptr;
        }

        return *regex_ptr;
    } catch (const std::regex_error& e) {
        LOG_F(ERROR, "Regex compilation error for pattern '{}': {}", pattern,
              e.what());
        throw;
    }
}

auto AsyncGlob::fnmatch(const fs::path& name,
                        std::string_view pattern) const noexcept -> bool {
    try {
        LOG_F(INFO, "AsyncGlob::fnmatch called with name: {}, pattern: {}",
              name.string(), pattern);

        bool result = std::regex_match(name.string(), compilePattern(pattern));
        LOG_F(INFO, "AsyncGlob::fnmatch returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in fnmatch: {}", e.what());
        return false;
    }
}

auto AsyncGlob::filter(std::span<const fs::path> names,
                       std::string_view pattern) const
    -> std::vector<fs::path> {
    LOG_F(INFO, "AsyncGlob::filter called with pattern: {}", pattern);

    try {
        auto compiled_pattern = compilePattern(pattern);
        std::vector<fs::path> result;
        result.reserve(names.size() / 2);  // Reasonable estimation

        // If we have a thread pool with multiple threads, use parallel
        // processing
        if (thread_pool_ && thread_pool_->size() > 1 && names.size() > 100) {
            const size_t chunk_size =
                (names.size() + thread_pool_->size() - 1) /
                thread_pool_->size();
            std::vector<std::future<std::vector<fs::path>>> futures;

            for (size_t i = 0; i < names.size(); i += chunk_size) {
                const size_t end = std::min(i + chunk_size, names.size());
                futures.push_back(std::async(std::launch::async, [&, i, end]() {
                    std::vector<fs::path> chunk_result;
                    for (size_t j = i; j < end; ++j) {
                        if (std::regex_match(names[j].string(),
                                             compiled_pattern)) {
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
            // Sequential processing for smaller datasets
            for (const auto& name : names) {
                if (std::regex_match(name.string(), compiled_pattern)) {
                    result.push_back(name);
                }
            }
        }

        LOG_F(INFO, "AsyncGlob::filter returning {} paths", result.size());
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in filter: {}", e.what());
        throw;
    }
}

auto AsyncGlob::expandTilde(const fs::path& path) const -> fs::path {
    LOG_F(INFO, "AsyncGlob::expandTilde called with path: {}", path.string());

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
                LOG_F(ERROR,
                      "Unable to expand `~` - HOME environment variable not "
                      "set.");
                THROW_INVALID_ARGUMENT(
                    "error: Unable to expand `~` - HOME environment variable "
                    "not set.");
            }

            pathStr = std::string(home) + pathStr.substr(1);
            fs::path expandedPath(pathStr);
            LOG_F(INFO, "Expanded path: {}", expandedPath.string());
            return expandedPath;
        }
        return path;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in expandTilde: {}", e.what());
        throw;
    }
}

auto AsyncGlob::hasMagic(std::string_view pathname) noexcept -> bool {
    LOG_F(INFO, "AsyncGlob::hasMagic called with pathname: {}", pathname);

    // Quick check without regex for better performance
    bool result = pathname.find_first_of("*?[") != std::string_view::npos;

    LOG_F(INFO, "AsyncGlob::hasMagic returning: {}", result);
    return result;
}

auto AsyncGlob::isHidden(std::string_view pathname) noexcept -> bool {
    LOG_F(INFO, "AsyncGlob::isHidden called with pathname: {}", pathname);

    // Check if pathname starts with a dot
    if (pathname.empty()) {
        return false;
    }

    size_t lastSlash = pathname.rfind('/');
    if (lastSlash == std::string_view::npos) {
        // No slash, check if starts with .
        bool result =
            pathname[0] == '.' && pathname.size() > 1 && pathname[1] != '.';
        LOG_F(INFO, "AsyncGlob::isHidden returning: {}", result);
        return result;
    } else {
        // Check if filename part starts with .
        size_t filenameStart = lastSlash + 1;
        if (filenameStart >= pathname.size()) {
            return false;
        }
        bool result = pathname[filenameStart] == '.' &&
                      (pathname.size() > filenameStart + 1) &&
                      pathname[filenameStart + 1] != '.';
        LOG_F(INFO, "AsyncGlob::isHidden returning: {}", result);
        return result;
    }
}

auto AsyncGlob::isRecursive(std::string_view pattern) noexcept -> bool {
    LOG_F(INFO, "AsyncGlob::isRecursive called with pattern: {}", pattern);
    bool result = pattern == "**";
    LOG_F(INFO, "AsyncGlob::isRecursive returning: {}", result);
    return result;
}

std::vector<fs::path> AsyncGlob::glob_sync(std::string_view pathname,
                                           bool recursive, bool dironly) {
    LOG_F(INFO, "AsyncGlob::glob_sync called with pathname: {}", pathname);

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

// Implementation of the glob method template is in the header
// The implementation details follow the same pattern as the original code,
// with optimizations applied for exception handling, boundary checking,
// and leveraging C++20 features.

}  // namespace atom::io

/*
 * fnmatch.cpp
 *
 * Copyright (C) 2023-2024 MaxQ <lightapttech.com>
 */

#include "fnmatch.hpp"

#include <algorithm>
#include <cctype>
#include <execution>
#include <list>
#include <memory>
#include <mutex>
#include <regex>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#else
#include <fnmatch.h>
#endif

#include <spdlog/spdlog.h>

#ifdef ATOM_USE_BOOST
#include <boost/regex.hpp>
#endif

#ifdef __SSE4_2__
#include <smmintrin.h>
#endif

namespace atom::algorithm {

namespace {
class PatternCache {
private:
    struct CacheEntry {
        std::string pattern;
        int flags;
        std::shared_ptr<std::regex> regex;
        std::chrono::steady_clock::time_point last_used;
    };

    static constexpr size_t MAX_CACHE_SIZE = 128;

    mutable std::mutex cache_mutex_;
    std::list<CacheEntry> entries_;
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> lookup_;

public:
    PatternCache() = default;

    std::shared_ptr<std::regex> get_regex(std::string_view pattern, int flags) {
        const std::string pattern_key =
            std::string(pattern) + ":" + std::to_string(flags);

        std::lock_guard<std::mutex> lock(cache_mutex_);

        auto it = lookup_.find(pattern_key);
        if (it != lookup_.end()) {
            auto entry_it = it->second;
            entry_it->last_used = std::chrono::steady_clock::now();
            entries_.splice(entries_.begin(), entries_, entry_it);
            return entry_it->regex;
        }

        std::string regex_str;
        auto result = translate(pattern, flags);
        if (!result) {
            throw FnmatchException("Failed to translate pattern to regex");
        }

        regex_str = std::move(result.value());

        std::shared_ptr<std::regex> new_regex;
        try {
            int regex_flags = std::regex::ECMAScript;
            if (flags & flags::CASEFOLD) {
                regex_flags |= std::regex::icase;
            }
            new_regex = std::make_shared<std::regex>(
                regex_str, static_cast<std::regex::flag_type>(regex_flags));
        } catch (const std::regex_error& e) {
            throw FnmatchException("Invalid regex pattern: " +
                                   std::string(e.what()));
        }

        CacheEntry entry{.pattern = std::string(pattern),
                         .flags = flags,
                         .regex = new_regex,
                         .last_used = std::chrono::steady_clock::now()};

        entries_.push_front(entry);
        lookup_[pattern_key] = entries_.begin();

        if (entries_.size() > MAX_CACHE_SIZE) {
            auto oldest = std::prev(entries_.end());
            lookup_.erase(oldest->pattern + ":" +
                          std::to_string(oldest->flags));
            entries_.pop_back();
        }

        return new_regex;
    }
};

PatternCache& get_pattern_cache() {
    static PatternCache cache;
    return cache;
}

}  // namespace

template <StringLike T1, StringLike T2>
auto fnmatch(T1&& pattern, T2&& string, int flags) -> bool {
    spdlog::debug("fnmatch called with pattern: {}, string: {}, flags: {}",
                  std::string_view(pattern), std::string_view(string), flags);

    try {
        auto result = fnmatch_nothrow(std::forward<T1>(pattern),
                                      std::forward<T2>(string), flags);

        if (!result) {
            const char* error_msg = "Unknown error";
            switch (static_cast<int>(result.error().error())) {
                case static_cast<int>(FnmatchError::InvalidPattern):
                    error_msg = "Invalid pattern";
                    break;
                case static_cast<int>(FnmatchError::UnmatchedBracket):
                    error_msg = "Unmatched bracket in pattern";
                    break;
                case static_cast<int>(FnmatchError::EscapeAtEnd):
                    error_msg = "Escape character at end of pattern";
                    break;
                case static_cast<int>(FnmatchError::InternalError):
                    error_msg = "Internal error during matching";
                    break;
            }
            throw FnmatchException(error_msg);
        }

        return result.value();
    } catch (const std::exception& e) {
        spdlog::error("Exception in fnmatch: {}", e.what());
        throw FnmatchException(e.what());
    } catch (...) {
        throw FnmatchException("Unknown error occurred");
    }
}

template <StringLike T1, StringLike T2>
auto fnmatch_nothrow(T1&& pattern, T2&& string, int flags) noexcept
    -> atom::type::expected<bool, FnmatchError> {
    const std::string_view pattern_view(pattern);
    const std::string_view string_view(string);

    if (pattern_view.empty()) {
        return string_view.empty();
    }

#ifdef ATOM_USE_BOOST
    try {
        auto translated = translate(pattern_view, flags);
        if (!translated) {
            return atom::type::unexpected(translated.error());
        }

        boost::regex::flag_type regex_flags = boost::regex::ECMAScript;
        if (flags & flags::CASEFOLD) {
            regex_flags |= boost::regex::icase;
        }

        boost::regex regex(translated.value(), regex_flags);
        bool result = boost::regex_match(
            std::string(string_view.begin(), string_view.end()), regex);

        spdlog::debug("Boost regex match result: {}", result);
        return result;
    } catch (...) {
        spdlog::error("Exception in Boost regex implementation");
        return atom::type::unexpected(FnmatchError::InternalError);
    }
#else
#ifdef _WIN32
    try {
        auto regex = get_pattern_cache().get_regex(pattern_view, flags);

        if (std::regex_match(
                std::string(string_view.begin(), string_view.end()), *regex)) {
            spdlog::debug("Regex match successful");
            return true;
        }

        return false;
    } catch (...) {
        spdlog::warn("Regex failed, falling back to manual implementation");

        auto p = pattern_view.begin();
        auto s = string_view.begin();

        while (p != pattern_view.end() && s != string_view.end()) {
            const char current_char = *p;
            switch (current_char) {
                case '?': {
                    ++s;
                    ++p;
                    break;
                }
                case '*': {
                    if (++p == pattern_view.end()) {
                        return true;
                    }

                    auto check_wildcards = [](auto start, auto end) {
                        return std::any_of(start, end, [](char c) {
                            return c == '*' || c == '?' || c == '[';
                        });
                    };

                    if (!check_wildcards(p, pattern_view.end())) {
                        const auto suffix_len =
                            static_cast<std::ptrdiff_t>(pattern_view.end() - p);
                        const auto remaining_len =
                            static_cast<std::ptrdiff_t>(string_view.end() - s);
                        if (suffix_len > remaining_len) {
                            return false;
                        }

                        const bool match = std::equal(
                            pattern_view.end() - suffix_len, pattern_view.end(),
                            string_view.end() - suffix_len,
                            [flags](char a, char b) {
                                return (flags & flags::CASEFOLD)
                                           ? (std::tolower(a) ==
                                              std::tolower(b))
                                           : (a == b);
                            });
                        return match;
                    }

                    while (s != string_view.end()) {
                        auto inner_result = fnmatch_nothrow(
                            std::string_view(p, pattern_view.end() - p),
                            std::string_view(s, string_view.end() - s), flags);

                        if (!inner_result) {
                            return inner_result;
                        }

                        if (inner_result.value()) {
                            return true;
                        }
                        ++s;
                    }
                    return false;
                }
                case '[': {
                    ++p;
                    break;
                }
                case '\\': {
                    if ((flags & flags::NOESCAPE) == 0) {
                        if (++p == pattern_view.end()) {
                            return atom::type::unexpected(
                                FnmatchError::EscapeAtEnd);
                        }
                    }
                    [[fallthrough]];
                }
                default: {
                    if ((flags & flags::CASEFOLD)
                            ? (std::tolower(*p) != std::tolower(*s))
                            : (*p != *s)) {
                        return false;
                    }
                    ++s;
                    ++p;
                    break;
                }
            }
        }

        while (p != pattern_view.end() && *p == '*') {
            ++p;
        }

        const bool result = p == pattern_view.end() && s == string_view.end();
        return result;
    }
#else
    try {
        const std::string pattern_str(pattern_view);
        const std::string string_str(string_view);

        int ret = ::fnmatch(pattern_str.c_str(), string_str.c_str(), flags);
        bool result = (ret == 0);
        spdlog::debug("System fnmatch result: {}", result);
        return result;
    } catch (...) {
        return atom::type::unexpected(FnmatchError::InternalError);
    }
#endif
#endif
}

template <std::ranges::input_range Range, StringLike Pattern>
    requires StringLike<std::ranges::range_value_t<Range>>
auto filter(const Range& names, Pattern&& pattern, int flags) -> bool {
    spdlog::debug("Filter called with pattern: {}", std::string_view(pattern));

    try {
        return std::ranges::any_of(names, [&pattern, flags](const auto& name) {
            try {
                bool match = fnmatch(pattern, name, flags);
                return match;
            } catch (const std::exception& e) {
                spdlog::error("Exception while matching name: {}", e.what());
                return false;
            }
        });
    } catch (const std::exception& e) {
        spdlog::error("Exception in filter: {}", e.what());
        throw FnmatchException(std::string("Filter operation failed: ") +
                               e.what());
    }
}

template <std::ranges::input_range Range, std::ranges::input_range PatternRange>
    requires StringLike<std::ranges::range_value_t<Range>> &&
             StringLike<std::ranges::range_value_t<PatternRange>>
auto filter(const Range& names, const PatternRange& patterns, int flags,
            bool use_parallel)
    -> std::vector<std::ranges::range_value_t<Range>> {
    using result_type = std::ranges::range_value_t<Range>;
    spdlog::debug("Filter called with multiple patterns and {} names",
                  std::ranges::distance(names));

    std::vector<result_type> result;

    try {
        const auto names_size = std::ranges::distance(names);
        result.reserve(std::min(static_cast<size_t>(names_size),
                                static_cast<size_t>(128)));

        std::vector<std::string_view> pattern_views;
        pattern_views.reserve(std::ranges::distance(patterns));
        for (const auto& p : patterns) {
            pattern_views.emplace_back(p);
        }

        std::mutex result_mutex;

        auto process_name = [&](const auto& name) {
            bool matched = false;
            const std::string_view name_view(name);

            if (use_parallel && pattern_views.size() > 4) {
                matched = std::any_of(
                    std::execution::par_unseq, pattern_views.begin(),
                    pattern_views.end(),
                    [&name_view, flags](const std::string_view& pattern) {
                        auto match_result =
                            fnmatch_nothrow(pattern, name_view, flags);
                        return match_result && match_result.value();
                    });
            } else {
                matched = std::ranges::any_of(
                    pattern_views,
                    [&name_view, flags](const std::string_view& pattern) {
                        auto match_result =
                            fnmatch_nothrow(pattern, name_view, flags);
                        return match_result && match_result.value();
                    });
            }

            if (matched) {
                std::lock_guard<std::mutex> lock(result_mutex);
                result.push_back(name);
            }
        };

        if (use_parallel && names_size > 100) {
            std::for_each(std::execution::par_unseq, std::ranges::begin(names),
                          std::ranges::end(names), process_name);
        } else {
            std::ranges::for_each(names, process_name);
        }

        spdlog::debug("Filter result contains {} matched names", result.size());
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Exception in multiple patterns filter: {}", e.what());
        throw FnmatchException(std::string("Multi-pattern filter failed: ") +
                               e.what());
    }
}

template <StringLike Pattern>
auto translate(Pattern&& pattern, int flags) noexcept
    -> atom::type::expected<std::string, FnmatchError> {
    const std::string_view pattern_view(pattern);
    spdlog::debug("Translating pattern: {} with flags: {}", pattern_view,
                  flags);

    std::string result;
    result.reserve(pattern_view.size() * 2);

    try {
        for (auto it = pattern_view.begin(); it != pattern_view.end(); ++it) {
            switch (*it) {
                case '*':
                    result += ".*";
                    break;

                case '?':
                    result += '.';
                    break;

                case '[': {
                    result += '[';
                    if (++it == pattern_view.end()) {
                        return atom::type::unexpected(
                            FnmatchError::UnmatchedBracket);
                    }

                    if (*it == '!' || *it == '^') {
                        result += '^';
                        ++it;
                    }

                    if (it == pattern_view.end()) {
                        return atom::type::unexpected(
                            FnmatchError::UnmatchedBracket);
                    }

                    if (*it == ']') {
                        result += *it;
                        ++it;
                        if (it == pattern_view.end()) {
                            return atom::type::unexpected(
                                FnmatchError::UnmatchedBracket);
                        }
                    }

                    while (it != pattern_view.end() && *it != ']') {
                        if (*it == '-' && it + 1 != pattern_view.end() &&
                            *(it + 1) != ']') {
                            result += *it++;
                            if (it == pattern_view.end()) {
                                return atom::type::unexpected(
                                    FnmatchError::UnmatchedBracket);
                            }
                            result += *it;
                        } else {
                            result += *it;
                        }
                    }

                    if (it == pattern_view.end()) {
                        return atom::type::unexpected(
                            FnmatchError::UnmatchedBracket);
                    }

                    result += ']';
                    break;
                }

                case '\\':
                    if ((flags & flags::NOESCAPE) == 0) {
                        if (++it == pattern_view.end()) {
                            return atom::type::unexpected(
                                FnmatchError::EscapeAtEnd);
                        }
                    }
                    [[fallthrough]];

                default:
                    if ((flags & flags::CASEFOLD) && std::isalpha(*it)) {
                        result += '[';
                        result += static_cast<char>(std::tolower(*it));
                        result += static_cast<char>(std::toupper(*it));
                        result += ']';
                    } else {
                        result += *it;
                    }
                    break;
            }
        }
        spdlog::debug("Translation successful. Resulting regex: {}", result);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Exception in translate: {}", e.what());
        return atom::type::unexpected(FnmatchError::InternalError);
    }
}

template bool atom::algorithm::fnmatch<std::string, std::string>(std::string&&,
                                                                 std::string&&,
                                                                 int);
template atom::type::expected<bool, atom::algorithm::FnmatchError>
atom::algorithm::fnmatch_nothrow<std::string, std::string>(std::string&&,
                                                           std::string&&,
                                                           int) noexcept;
template atom::type::expected<std::string, atom::algorithm::FnmatchError>
atom::algorithm::translate<std::string>(std::string&&, int) noexcept;
template bool atom::algorithm::filter<std::vector<std::string>, std::string>(
    const std::vector<std::string>&, std::string&&, int);
template std::vector<std::string>
atom::algorithm::filter<std::vector<std::string>, std::vector<std::string>>(
    const std::vector<std::string>&, const std::vector<std::string>&, int,
    bool);

}  // namespace atom::algorithm
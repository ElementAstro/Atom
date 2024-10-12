/*
 * fnmatch.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-2

Description: Python Like fnmatch for C++

**************************************************/

#include "fnmatch.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#else
#include <fnmatch.h>
#endif

namespace atom::algorithm {

#ifdef _WIN32
constexpr int FNM_NOESCAPE = 0x01;
constexpr int FNM_PATHNAME = 0x02;
constexpr int FNM_PERIOD = 0x04;
constexpr int FNM_CASEFOLD = 0x08;
#endif

auto fnmatch(std::string_view pattern, std::string_view string,
             int flags) -> bool {
#ifdef _WIN32
    auto p = pattern.begin();
    auto s = string.begin();

    while (p != pattern.end() && s != string.end()) {
        switch (*p) {
            case '?':
                ++s;
                break;
            case '*':
                if (++p == pattern.end()) {
                    return true;
                }
                while (s != string.end()) {
                    if (fnmatch({p, pattern.end()}, {s, string.end()}, flags)) {
                        return true;
                    }
                    ++s;
                }
                return false;
            case '[': {
                if (++p == pattern.end()) {
                    return false;
                }
                bool invert = false;
                if (*p == '!') {
                    invert = true;
                    ++p;
                }
                bool matched = false;
                char last_char = 0;
                while (p != pattern.end() && *p != ']') {
                    if (*p == '-' && last_char != 0 && p + 1 != pattern.end() &&
                        *(p + 1) != ']') {
                        ++p;
                        if (*s >= last_char && *s <= *p) {
                            matched = true;
                            break;
                        }
                    } else {
                        if (*s == *p) {
                            matched = true;
                            break;
                        }
                        last_char = *p;
                    }
                    ++p;
                }
                if (invert) {
                    matched = !matched;
                }
                if (!matched) {
                    return false;
                }
                ++s;
                break;
            }
            case '\\':
                if (!(flags & FNM_NOESCAPE) && ++p == pattern.end()) {
                    return false;
                }
                [[fallthrough]];
            default:
                if ((flags & FNM_CASEFOLD)
                        ? (std::tolower(*p) != std::tolower(*s))
                        : (*p != *s)) {
                    return false;
                }
                ++s;
                break;
        }
        ++p;
    }

    if (p == pattern.end() && s == string.end()) {
        return true;
    }
    if (p != pattern.end() && *p == '*') {
        ++p;
    }
    return p == pattern.end() && s == string.end();
#else
    return ::fnmatch(pattern.data(), string.data(), flags) == 0;
#endif
}

auto filter(const std::vector<std::string>& names, std::string_view pattern,
            int flags) -> bool {
    return std::ranges::any_of(names, [&](const std::string& name) {
        return fnmatch(pattern, name, flags);
    });
}

auto filter(const std::vector<std::string>& names,
            const std::vector<std::string>& patterns,
            int flags) -> std::vector<std::string> {
    std::vector<std::string> result;
    for (const auto& name : names) {
        if (std::ranges::any_of(patterns, [&](std::string_view pattern) {
                return fnmatch(pattern, name, flags);
            })) {
            result.push_back(name);
        }
    }
    return result;
}

auto translate(std::string_view pattern, std::string& result,
               int flags) -> bool {
    result.clear();
    for (auto it = pattern.begin(); it != pattern.end(); ++it) {
        switch (*it) {
            case '*':
                result += ".*";
                break;
            case '?':
                result += '.';
                break;
            case '[': {
                result += '[';
                if (++it == pattern.end()) {
                    return false;
                }
                if (*it == '!') {
                    result += '^';
                    ++it;
                }
                if (it == pattern.end()) {
                    return false;
                }
                char lastChar = *it;
                result += *it;
                while (++it != pattern.end() && *it != ']') {
                    if (*it == '-' && it + 1 != pattern.end() &&
                        *(it + 1) != ']') {
                        result += *it;
                        result += *(++it);
                        lastChar = *it;
                    } else {
                        result += *it;
                        lastChar = *it;
                    }
                }
                result += ']';
                break;
            }
            case '\\':
                if (!(flags & FNM_NOESCAPE) && ++it == pattern.end()) {
                    return false;
                }
                [[fallthrough]];
            default:
                if ((flags & FNM_CASEFOLD) && std::isalpha(*it)) {
                    result += '[';
                    result += std::tolower(*it);
                    result += std::toupper(*it);
                    result += ']';
                } else {
                    result += *it;
                }
                break;
        }
    }
    return true;
}

}  // namespace atom::algorithm

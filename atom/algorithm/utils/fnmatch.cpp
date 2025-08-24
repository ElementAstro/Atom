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

// Template function definitions moved to header file

// Multi-pattern filter template function moved to header file

// Translate template function moved to header file

// All template instantiations removed - functions are now header-only templates

}  // namespace atom::algorithm

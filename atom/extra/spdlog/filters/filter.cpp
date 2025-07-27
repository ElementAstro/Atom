#include "filter.h"

#include <algorithm>
#include <mutex>
#include <chrono>

namespace modern_log {

void LogFilter::add_filter(FilterFunc filter) {
    std::unique_lock lock(mutex_);
    filters_.push_back(std::move(filter));
    clear_cache(); // Clear cache when filters change
}

void LogFilter::clear_filters() {
    std::unique_lock lock(mutex_);
    filters_.clear();
    clear_cache(); // Clear cache when filters change
}

bool LogFilter::should_log(std::string_view message, Level level,
                           const LogContext& ctx) const {
    // Fast path: if no filters, always log
    {
        std::shared_lock lock(mutex_);
        if (filters_.empty()) {
            return true;
        }
    }

    // Check cache if enabled
    if (cache_enabled_.load()) {
        size_t cache_key = generate_cache_key(message, level, ctx);

        {
            std::shared_lock cache_lock(cache_mutex_);
            auto it = filter_cache_.find(cache_key);
            if (it != filter_cache_.end() && is_cache_result_valid(it->second)) {
                cache_hits_.fetch_add(1);
                it->second.access_count++;
                return it->second.should_log;
            }
        }

        cache_misses_.fetch_add(1);
    }

    // Evaluate filters
    bool result = should_log_fast(message, level, ctx);

    // Cache the result if caching is enabled
    if (cache_enabled_.load()) {
        size_t cache_key = generate_cache_key(message, level, ctx);
        std::unique_lock cache_lock(cache_mutex_);

        // Check cache size and cleanup if needed
        if (filter_cache_.size() >= cache_max_size_.load()) {
            cleanup_cache();
        }

        filter_cache_[cache_key] = FilterResult{
            result,
            std::chrono::steady_clock::now(),
            1
        };
    }

    return result;
}

bool LogFilter::should_log_fast(std::string_view message, Level level,
                                const LogContext& ctx) const {
    std::shared_lock lock(mutex_);
    return std::ranges::all_of(filters_, [&](const auto& filter) {
        return filter(message, level, ctx);
    });
}

size_t LogFilter::filter_count() const {
    std::shared_lock lock(mutex_);
    return filters_.size();
}

void LogFilter::set_cache_enabled(bool enabled) {
    cache_enabled_.store(enabled);
    if (!enabled) {
        clear_cache();
    }
}

void LogFilter::set_cache_max_size(size_t max_size) {
    cache_max_size_.store(max_size);
}

void LogFilter::set_cache_ttl(std::chrono::milliseconds ttl) {
    cache_ttl_.store(ttl);
}

void LogFilter::clear_cache() {
    std::unique_lock cache_lock(cache_mutex_);
    filter_cache_.clear();
    cache_hits_.store(0);
    cache_misses_.store(0);
}

std::pair<size_t, size_t> LogFilter::get_cache_stats() const {
    std::shared_lock cache_lock(cache_mutex_);
    return {filter_cache_.size(), cache_hits_.load()};
}

size_t LogFilter::generate_cache_key(std::string_view message, Level level,
                                     const LogContext& ctx) const {
    size_t h1 = std::hash<std::string_view>{}(message);
    size_t h2 = std::hash<int>{}(static_cast<int>(level));
    size_t h3 = ctx.hash();

    // Combine hashes
    return h1 ^ (h2 << 1) ^ (h3 << 2);
}

bool LogFilter::is_cache_result_valid(const FilterResult& result) const {
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - result.timestamp);
    return age < cache_ttl_.load();
}

void LogFilter::cleanup_cache() const {
    auto now = std::chrono::steady_clock::now();
    auto ttl = cache_ttl_.load();

    auto it = filter_cache_.begin();
    while (it != filter_cache_.end()) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.timestamp);
        if (age >= ttl || it->second.access_count == 0) {
            it = filter_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace modern_log

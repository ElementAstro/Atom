#include "builtin_filters.h"

#include <unordered_map>
#include <unordered_set>

namespace modern_log {

LogFilter::FilterFunc BuiltinFilters::level_filter(Level min_level) {
    return [min_level](const std::string&, Level level, const LogContext&) {
        return level >= min_level;
    };
}

LogFilter::FilterFunc BuiltinFilters::regex_filter(const std::regex& pattern,
                                                   bool include) {
    return
        [pattern, include](const std::string& msg, Level, const LogContext&) {
            bool matches = std::regex_search(msg, pattern);
            return include ? matches : !matches;
        };
}

LogFilter::FilterFunc BuiltinFilters::rate_limit_filter(size_t max_per_second) {
    return [max_per_second, last_reset = std::chrono::steady_clock::now(),
            counter = size_t{0}](const std::string&, Level,
                                 const LogContext&) mutable {
        auto now = std::chrono::steady_clock::now();
        if (now - last_reset >= std::chrono::seconds(1)) {
            counter = 0;
            last_reset = now;
        }
        return ++counter <= max_per_second;
    };
}

LogFilter::FilterFunc BuiltinFilters::user_filter(
    const std::vector<std::string>& allowed_users) {
    auto user_set = std::make_shared<std::unordered_set<std::string>>(
        allowed_users.begin(), allowed_users.end());

    return [user_set](const std::string&, Level, const LogContext& ctx) {
        if (ctx.user_id().empty())
            return true;  // 允许没有用户ID的日志
        return user_set->contains(ctx.user_id());
    };
}

LogFilter::FilterFunc BuiltinFilters::time_window_filter(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end) {
    return [start, end](const std::string&, Level, const LogContext&) {
        auto now = std::chrono::system_clock::now();
        return now >= start && now <= end;
    };
}

LogFilter::FilterFunc BuiltinFilters::keyword_filter(
    const std::vector<std::string>& keywords, bool include) {
    auto keyword_set = std::make_shared<std::vector<std::string>>(keywords);

    return [keyword_set, include](const std::string& msg, Level,
                                  const LogContext&) {
        bool found =
            std::any_of(keyword_set->begin(), keyword_set->end(),
                        [&msg](const std::string& keyword) {
                            return msg.find(keyword) != std::string::npos;
                        });
        return include ? found : !found;
    };
}

LogFilter::FilterFunc BuiltinFilters::sampling_filter(double sample_rate) {
    return [sample_rate, counter = std::atomic<size_t>{0}](
               const std::string&, Level, const LogContext&) mutable {
        if (sample_rate >= 1.0)
            return true;
        if (sample_rate <= 0.0)
            return false;

        size_t current = counter.fetch_add(1);
        return (current % static_cast<size_t>(1.0 / sample_rate)) == 0;
    };
}

LogFilter::FilterFunc BuiltinFilters::duplicate_filter(
    std::chrono::seconds window) {
    using TimePoint = std::chrono::steady_clock::time_point;
    auto message_times =
        std::make_shared<std::unordered_map<std::string, TimePoint>>();
    auto mutex = std::make_shared<std::mutex>();

    return [message_times, mutex, window](const std::string& msg, Level,
                                          const LogContext&) {
        std::lock_guard lock(*mutex);
        auto now = std::chrono::steady_clock::now();

        // 清理过期的记录
        for (auto it = message_times->begin(); it != message_times->end();) {
            if (now - it->second > window) {
                it = message_times->erase(it);
            } else {
                ++it;
            }
        }

        // 检查是否重复
        if (auto it = message_times->find(msg); it != message_times->end()) {
            return false;  // 重复消息，过滤掉
        }

        (*message_times)[msg] = now;
        return true;
    };
}

}  // namespace modern_log

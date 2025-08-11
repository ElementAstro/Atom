#include "filter.h"

#include <algorithm>
#include <mutex>

namespace modern_log {

void LogFilter::add_filter(FilterFunc filter) {
    std::unique_lock lock(mutex_);
    filters_.push_back(std::move(filter));
}

void LogFilter::clear_filters() {
    std::unique_lock lock(mutex_);
    filters_.clear();
}

bool LogFilter::should_log(const std::string& message, Level level,
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

}  // namespace modern_log

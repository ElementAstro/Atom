/*
 * error_aggregator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Error aggregator implementation

**************************************************/

#include "error_aggregator.hpp"

#include <algorithm>
#include <ranges>

namespace atom::error {

ErrorAggregator::ErrorAggregator() = default;

void ErrorAggregator::addError(std::shared_ptr<ErrorContext> context) {
    if (!context) [[unlikely]] {
        return;
    }

    std::unique_lock lock(aggregationMutex_);
    std::string key = context->getCorrelationId();
    if (key.empty()) {
        key = std::to_string(context->getErrorCode());
    }

    aggregatedErrors_[key].emplace_back(std::move(context));
}

auto ErrorAggregator::getAggregatedErrors(const std::string& key) const
    -> std::vector<std::shared_ptr<ErrorContext>> {
    std::shared_lock lock(aggregationMutex_);
    if (auto it = aggregatedErrors_.find(key); it != aggregatedErrors_.end()) {
        return it->second;
    }
    return {};
}

auto ErrorAggregator::getAggregationKeys() const -> std::vector<std::string> {
    std::shared_lock lock(aggregationMutex_);
    std::vector<std::string> keys;
    keys.reserve(aggregatedErrors_.size());

    for (const auto& key : aggregatedErrors_ | std::views::keys) {
        keys.emplace_back(key);
    }

    return keys;
}

void ErrorAggregator::clear() {
    std::unique_lock lock(aggregationMutex_);
    aggregatedErrors_.clear();
}

void ErrorAggregator::clearOlderThan(std::chrono::minutes maxAge) {
    std::unique_lock lock(aggregationMutex_);
    const auto cutoff = std::chrono::system_clock::now() - maxAge;

    for (auto& [key, errors] : aggregatedErrors_) {
        std::erase_if(errors,
                      [&cutoff](const std::shared_ptr<ErrorContext>& ctx) {
                          return ctx && ctx->getTimestamp() < cutoff;
                      });
    }

    std::erase_if(aggregatedErrors_,
                  [](const auto& pair) { return pair.second.empty(); });
}

auto ErrorAggregator::getStatistics() const
    -> std::unordered_map<std::string, int> {
    std::shared_lock lock(aggregationMutex_);
    std::unordered_map<std::string, int> stats;

    stats.emplace("total_keys", static_cast<int>(aggregatedErrors_.size()));

    int totalErrors = 0;
    for (const auto& errors : aggregatedErrors_ | std::views::values) {
        totalErrors += static_cast<int>(errors.size());
    }
    stats.emplace("total_aggregated_errors", totalErrors);

    return stats;
}

}  // namespace atom::error

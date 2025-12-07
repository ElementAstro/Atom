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

namespace atom::error {

ErrorAggregator::ErrorAggregator() = default;

void ErrorAggregator::addError(std::shared_ptr<ErrorContext> context) {
    if (!context)
        return;

    std::unique_lock<std::shared_mutex> lock(aggregationMutex_);
    std::string key = context->getCorrelationId();
    if (key.empty()) {
        key = std::to_string(context->getErrorCode());
    }

    aggregatedErrors_[key].push_back(context);
}

auto ErrorAggregator::getAggregatedErrors(const std::string& key) const
    -> std::vector<std::shared_ptr<ErrorContext>> {
    std::shared_lock<std::shared_mutex> lock(aggregationMutex_);
    auto it = aggregatedErrors_.find(key);
    return it != aggregatedErrors_.end()
               ? it->second
               : std::vector<std::shared_ptr<ErrorContext>>{};
}

auto ErrorAggregator::getAggregationKeys() const -> std::vector<std::string> {
    std::shared_lock<std::shared_mutex> lock(aggregationMutex_);
    std::vector<std::string> keys;
    keys.reserve(aggregatedErrors_.size());

    for (const auto& [key, errors] : aggregatedErrors_) {
        keys.push_back(key);
    }

    return keys;
}

void ErrorAggregator::clear() {
    std::unique_lock<std::shared_mutex> lock(aggregationMutex_);
    aggregatedErrors_.clear();
}

void ErrorAggregator::clearOlderThan(std::chrono::minutes maxAge) {
    std::unique_lock<std::shared_mutex> lock(aggregationMutex_);
    auto cutoff = std::chrono::system_clock::now() - maxAge;

    for (auto it = aggregatedErrors_.begin(); it != aggregatedErrors_.end();) {
        auto& errors = it->second;
        errors.erase(
            std::remove_if(
                errors.begin(), errors.end(),
                [cutoff](const std::shared_ptr<ErrorContext>& context) {
                    return context && context->getTimestamp() < cutoff;
                }),
            errors.end());

        if (errors.empty()) {
            it = aggregatedErrors_.erase(it);
        } else {
            ++it;
        }
    }
}

auto ErrorAggregator::getStatistics() const
    -> std::unordered_map<std::string, int> {
    std::shared_lock<std::shared_mutex> lock(aggregationMutex_);
    std::unordered_map<std::string, int> stats;

    stats["total_keys"] = static_cast<int>(aggregatedErrors_.size());

    int totalErrors = 0;
    for (const auto& [key, errors] : aggregatedErrors_) {
        totalErrors += static_cast<int>(errors.size());
    }
    stats["total_aggregated_errors"] = totalErrors;

    return stats;
}

}  // namespace atom::error

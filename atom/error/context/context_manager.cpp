/*
 * context_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Error context manager implementation

**************************************************/

#include "context_manager.hpp"

#include <algorithm>

namespace atom::error {

auto ErrorContextManager::getInstance() -> ErrorContextManager& {
    static ErrorContextManager instance;
    return instance;
}

void ErrorContextManager::registerContext(
    std::shared_ptr<ErrorContext> context) {
    if (!context) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(contextsMutex_);
    contexts_[context->getErrorId()] = std::move(context);
}

auto ErrorContextManager::getContext(const ErrorId& errorId) const
    -> std::shared_ptr<ErrorContext> {
    std::shared_lock<std::shared_mutex> lock(contextsMutex_);
    auto it = contexts_.find(errorId);
    return it != contexts_.end() ? it->second : nullptr;
}

auto ErrorContextManager::getContextsByCorrelation(
    const std::string& correlationId) const
    -> std::vector<std::shared_ptr<ErrorContext>> {
    std::shared_lock<std::shared_mutex> lock(contextsMutex_);
    std::vector<std::shared_ptr<ErrorContext>> result;

    for (const auto& [id, context] : contexts_) {
        if (context && context->getCorrelationId() == correlationId) {
            result.push_back(context);
        }
    }

    return result;
}

auto ErrorContextManager::getStatistics() const
    -> std::unordered_map<std::string, int> {
    std::shared_lock<std::shared_mutex> lock(contextsMutex_);
    std::unordered_map<std::string, int> stats;

    stats["total_contexts"] = static_cast<int>(contexts_.size());

    std::unordered_map<ErrorSeverity, int> severityCounts;
    std::unordered_map<ErrorCategory, int> categoryCounts;

    for (const auto& [id, context] : contexts_) {
        if (context) {
            severityCounts[context->getSeverity()]++;
            categoryCounts[context->getCategory()]++;
        }
    }

    for (const auto& [severity, count] : severityCounts) {
        stats[std::string(severityToString(severity))] = count;
    }

    for (const auto& [category, count] : categoryCounts) {
        stats[std::string(categoryToString(category))] = count;
    }

    return stats;
}

void ErrorContextManager::cleanup(std::chrono::minutes maxAge) {
    std::unique_lock<std::shared_mutex> lock(contextsMutex_);
    auto cutoff = std::chrono::system_clock::now() - maxAge;

    for (auto it = contexts_.begin(); it != contexts_.end();) {
        if (it->second && it->second->getTimestamp() < cutoff) {
            it = contexts_.erase(it);
        } else {
            ++it;
        }
    }
}

void ErrorContextManager::clear() {
    std::unique_lock<std::shared_mutex> lock(contextsMutex_);
    contexts_.clear();
}

auto ErrorContextManager::size() const -> size_t {
    std::shared_lock<std::shared_mutex> lock(contextsMutex_);
    return contexts_.size();
}

}  // namespace atom::error

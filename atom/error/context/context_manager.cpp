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
#include <ranges>

namespace atom::error {

auto ErrorContextManager::getInstance() -> ErrorContextManager& {
    static ErrorContextManager instance;
    return instance;
}

void ErrorContextManager::registerContext(
    std::shared_ptr<ErrorContext> context) {
    if (!context) [[unlikely]] {
        return;
    }

    std::unique_lock lock(contextsMutex_);
    contexts_.insert_or_assign(context->getErrorId(), std::move(context));
}

auto ErrorContextManager::getContext(const ErrorId& errorId) const
    -> std::shared_ptr<ErrorContext> {
    std::shared_lock lock(contextsMutex_);
    if (auto it = contexts_.find(errorId); it != contexts_.end()) {
        return it->second;
    }
    return nullptr;
}

auto ErrorContextManager::getContextsByCorrelation(
    const std::string& correlationId) const
    -> std::vector<std::shared_ptr<ErrorContext>> {
    std::shared_lock lock(contextsMutex_);

    auto matchingContexts =
        contexts_ | std::views::values |
        std::views::filter([&correlationId](const auto& ctx) {
            return ctx && ctx->getCorrelationId() == correlationId;
        });

    return {matchingContexts.begin(), matchingContexts.end()};
}

auto ErrorContextManager::getStatistics() const
    -> std::unordered_map<std::string, int> {
    std::shared_lock lock(contextsMutex_);
    std::unordered_map<std::string, int> stats;
    std::unordered_map<ErrorSeverity, int> severityCounts;
    std::unordered_map<ErrorCategory, int> categoryCounts;

    stats.emplace("total_contexts", static_cast<int>(contexts_.size()));

    for (const auto& ctx : contexts_ | std::views::values) {
        if (ctx) [[likely]] {
            ++severityCounts[ctx->getSeverity()];
            ++categoryCounts[ctx->getCategory()];
        }
    }

    for (const auto& [severity, count] : severityCounts) {
        stats.emplace(severityToString(severity), count);
    }

    for (const auto& [category, count] : categoryCounts) {
        stats.emplace(categoryToString(category), count);
    }

    return stats;
}

void ErrorContextManager::cleanup(std::chrono::minutes maxAge) {
    std::unique_lock lock(contextsMutex_);
    const auto cutoff = std::chrono::system_clock::now() - maxAge;

    std::erase_if(contexts_, [&cutoff](const auto& pair) {
        const auto& [id, ctx] = pair;
        return ctx && ctx->getTimestamp() < cutoff;
    });
}

void ErrorContextManager::clear() {
    std::unique_lock lock(contextsMutex_);
    contexts_.clear();
}

auto ErrorContextManager::size() const -> size_t {
    std::shared_lock lock(contextsMutex_);
    return contexts_.size();
}

}  // namespace atom::error

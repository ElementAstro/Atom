/*
 * error_reporter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Error reporter implementation

**************************************************/

#include "error_reporter.hpp"

#include <format>
#include <iostream>
#include <ranges>

namespace atom::error {

ErrorReporter::ErrorReporter()
    : running_(false),
      maxQueueSize_(10000),
      aggregationStrategy_(AggregationStrategy::None),
      aggregationWindow_(std::chrono::milliseconds(1000)),
      totalErrors_(0),
      processedErrors_(0),
      filteredErrors_(0),
      droppedErrors_(0) {
    // Initialize severity stats
    for (int i = 0; i <= static_cast<int>(ErrorSeverity::Fatal); ++i) {
        severityStats_[static_cast<ErrorSeverity>(i)] = 0;
    }

    // Initialize category stats
    for (int i = 0; i <= static_cast<int>(ErrorCategory::External); ++i) {
        categoryStats_[static_cast<ErrorCategory>(i)] = 0;
    }
}

ErrorReporter::~ErrorReporter() { stop(); }

void ErrorReporter::start() {
    std::scoped_lock lock(queueMutex_);
    if (!running_.load()) {
        running_ = true;
        processingThread_ = std::thread(&ErrorReporter::processingLoop, this);
    }
}

void ErrorReporter::stop() {
    running_ = false;
    queueCondition_.notify_all();

    if (processingThread_.joinable()) {
        processingThread_.join();
    }
}

auto ErrorReporter::isRunning() const -> bool { return running_.load(); }

void ErrorReporter::reportError(std::shared_ptr<ErrorContext> context) {
    if (!context) [[unlikely]] {
        return;
    }

    ++totalErrors_;

    std::unique_lock lock(queueMutex_);

    // Check queue size limit
    if (errorQueue_.size() >= maxQueueSize_) [[unlikely]] {
        ++droppedErrors_;
        return;
    }

    errorQueue_.push(std::move(context));
    lock.unlock();

    queueCondition_.notify_one();
}

void ErrorReporter::addHandler(const std::string& name,
                               ErrorHandlerCallback handler) {
    std::unique_lock lock(handlersMutex_);
    handlers_.insert_or_assign(name, std::move(handler));
}

void ErrorReporter::removeHandler(const std::string& name) {
    std::unique_lock lock(handlersMutex_);
    handlers_.erase(name);
}

void ErrorReporter::addFilter(const std::string& name, ErrorFilter filter) {
    std::unique_lock lock(handlersMutex_);
    filters_.insert_or_assign(name, std::move(filter));
}

void ErrorReporter::removeFilter(const std::string& name) {
    std::unique_lock lock(handlersMutex_);
    filters_.erase(name);
}

void ErrorReporter::setAggregationStrategy(AggregationStrategy strategy) {
    std::scoped_lock lock(aggregationMutex_);
    aggregationStrategy_ = strategy;
}

void ErrorReporter::setAggregationWindow(std::chrono::milliseconds window) {
    std::scoped_lock lock(aggregationMutex_);
    aggregationWindow_ = window;
}

auto ErrorReporter::getStatistics() const
    -> std::unordered_map<std::string, int> {
    std::unordered_map<std::string, int> stats;

    stats["total_errors"] = totalErrors_.load();
    stats["processed_errors"] = processedErrors_.load();
    stats["filtered_errors"] = filteredErrors_.load();
    stats["dropped_errors"] = droppedErrors_.load();
    stats["queue_size"] = static_cast<int>(getQueueSize());

    // Severity statistics
    for (const auto& [severity, count] : severityStats_) {
        stats[std::string(severityToString(severity))] = count.load();
    }

    // Category statistics
    for (const auto& [category, count] : categoryStats_) {
        stats[std::string(categoryToString(category))] = count.load();
    }

    return stats;
}

void ErrorReporter::clearStatistics() {
    totalErrors_.store(0);
    processedErrors_.store(0);
    filteredErrors_.store(0);
    droppedErrors_.store(0);

    for (auto& count : severityStats_ | std::views::values) {
        count.store(0);
    }

    for (auto& count : categoryStats_ | std::views::values) {
        count.store(0);
    }
}

void ErrorReporter::setMaxQueueSize(size_t maxSize) {
    std::scoped_lock lock(queueMutex_);
    maxQueueSize_ = maxSize;
}

auto ErrorReporter::getQueueSize() const -> size_t {
    std::scoped_lock lock(queueMutex_);
    return errorQueue_.size();
}

void ErrorReporter::processingLoop() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(queueMutex_);

        queueCondition_.wait(
            lock, [this] { return !errorQueue_.empty() || !running_.load(); });

        while (!errorQueue_.empty()) {
            auto context = errorQueue_.front();
            errorQueue_.pop();
            lock.unlock();

            processError(context);

            lock.lock();
        }
    }
}

void ErrorReporter::processError(std::shared_ptr<ErrorContext> context) {
    if (!context) [[unlikely]] {
        return;
    }

    // Apply filters
    if (!shouldProcess(context)) {
        ++filteredErrors_;
        return;
    }

    // Update statistics
    updateStatistics(context);

    // Handle aggregation
    if (aggregationStrategy_ != AggregationStrategy::None) {
        aggregateError(context);
    }

    // Call handlers
    std::shared_lock lock(handlersMutex_);
    for (const auto& [name, handler] : handlers_) {
        try {
            handler(context);
        } catch (const std::exception& e) {
            std::cerr << std::format("Error in handler '{}': {}\n", name,
                                     e.what());
        }
    }

    ++processedErrors_;
}

bool ErrorReporter::shouldProcess(std::shared_ptr<ErrorContext> context) {
    std::shared_lock lock(handlersMutex_);

    for (const auto& [name, filter] : filters_) {
        try {
            if (!filter(context)) {
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << std::format("Error in filter '{}': {}\n", name,
                                     e.what());
        }
    }

    return true;
}

void ErrorReporter::updateStatistics(std::shared_ptr<ErrorContext> context) {
    ++severityStats_[context->getSeverity()];
    ++categoryStats_[context->getCategory()];
}

void ErrorReporter::aggregateError(std::shared_ptr<ErrorContext> context) {
    std::scoped_lock lock(aggregationMutex_);

    const auto key = getAggregationKey(context);
    const auto now = std::chrono::steady_clock::now();

    // Check if we need to flush old aggregated errors
    if (auto it = aggregationTimestamps_.find(key);
        it != aggregationTimestamps_.end()) {
        if (now - it->second > aggregationWindow_) {
            aggregatedErrors_[key].clear();
            it->second = now;
        }
    } else {
        aggregationTimestamps_.emplace(key, now);
    }

    aggregatedErrors_[key].emplace_back(std::move(context));
}

std::string ErrorReporter::getAggregationKey(
    std::shared_ptr<ErrorContext> context) {
    switch (aggregationStrategy_) {
        case AggregationStrategy::BySeverity:
            return std::string(severityToString(context->getSeverity()));
        case AggregationStrategy::ByCategory:
            return std::string(categoryToString(context->getCategory()));
        case AggregationStrategy::ByCode:
            return std::to_string(context->getErrorCode());
        case AggregationStrategy::ByCorrelation:
            return context->getCorrelationId();
        case AggregationStrategy::ByTimeWindow: {
            auto timestamp = context->getTimestamp();
            auto time_t = std::chrono::system_clock::to_time_t(timestamp);
            auto window_seconds =
                std::chrono::duration_cast<std::chrono::seconds>(
                    aggregationWindow_)
                    .count();
            auto window_start = (time_t / window_seconds) * window_seconds;
            return std::to_string(window_start);
        }
        default:
            return context->getErrorId();
    }
}

}  // namespace atom::error

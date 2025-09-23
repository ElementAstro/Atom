/*
 * error_handler.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Implementation of thread-safe error handling system

**************************************************/

#include "error_handler.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <exception>

namespace atom::error {

// ErrorReporter implementation
ErrorReporter::ErrorReporter()
    : running_(false)
    , maxQueueSize_(10000)
    , aggregationStrategy_(AggregationStrategy::None)
    , aggregationWindow_(std::chrono::milliseconds(1000))
    , totalErrors_(0)
    , processedErrors_(0)
    , filteredErrors_(0)
    , droppedErrors_(0) {
    
    // Initialize severity stats
    for (int i = 0; i <= static_cast<int>(ErrorSeverity::Fatal); ++i) {
        severityStats_[static_cast<ErrorSeverity>(i)] = 0;
    }
    
    // Initialize category stats
    for (int i = 0; i <= static_cast<int>(ErrorCategory::External); ++i) {
        categoryStats_[static_cast<ErrorCategory>(i)] = 0;
    }
}

ErrorReporter::~ErrorReporter() {
    stop();
}

void ErrorReporter::start() {
    std::lock_guard<std::mutex> lock(queueMutex_);
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

auto ErrorReporter::isRunning() const -> bool {
    return running_.load();
}

void ErrorReporter::reportError(std::shared_ptr<ErrorContext> context) {
    if (!context) return;
    
    totalErrors_++;
    
    std::unique_lock<std::mutex> lock(queueMutex_);
    
    // Check queue size limit
    if (errorQueue_.size() >= maxQueueSize_) {
        droppedErrors_++;
        return;
    }
    
    errorQueue_.push(context);
    lock.unlock();
    
    queueCondition_.notify_one();
}

void ErrorReporter::addHandler(const std::string& name, ErrorHandlerCallback handler) {
    std::unique_lock<std::shared_mutex> lock(handlersMutex_);
    handlers_[name] = std::move(handler);
}

void ErrorReporter::removeHandler(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(handlersMutex_);
    handlers_.erase(name);
}

void ErrorReporter::addFilter(const std::string& name, ErrorFilter filter) {
    std::unique_lock<std::shared_mutex> lock(handlersMutex_);
    filters_[name] = std::move(filter);
}

void ErrorReporter::removeFilter(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(handlersMutex_);
    filters_.erase(name);
}

void ErrorReporter::setAggregationStrategy(AggregationStrategy strategy) {
    std::lock_guard<std::mutex> lock(aggregationMutex_);
    aggregationStrategy_ = strategy;
}

void ErrorReporter::setAggregationWindow(std::chrono::milliseconds window) {
    std::lock_guard<std::mutex> lock(aggregationMutex_);
    aggregationWindow_ = window;
}

auto ErrorReporter::getStatistics() const -> std::unordered_map<std::string, int> {
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
    totalErrors_ = 0;
    processedErrors_ = 0;
    filteredErrors_ = 0;
    droppedErrors_ = 0;
    
    for (auto& [severity, count] : severityStats_) {
        count = 0;
    }
    
    for (auto& [category, count] : categoryStats_) {
        count = 0;
    }
}

void ErrorReporter::setMaxQueueSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    maxQueueSize_ = maxSize;
}

auto ErrorReporter::getQueueSize() const -> size_t {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return errorQueue_.size();
}

void ErrorReporter::processingLoop() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        queueCondition_.wait(lock, [this] {
            return !errorQueue_.empty() || !running_.load();
        });
        
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
    if (!context) return;
    
    // Apply filters
    if (!shouldProcess(context)) {
        filteredErrors_++;
        return;
    }
    
    // Update statistics
    updateStatistics(context);
    
    // Handle aggregation
    if (aggregationStrategy_ != AggregationStrategy::None) {
        aggregateError(context);
    }
    
    // Call handlers
    std::shared_lock<std::shared_mutex> lock(handlersMutex_);
    for (const auto& [name, handler] : handlers_) {
        try {
            handler(context);
        } catch (const std::exception& e) {
            // Log handler error but continue processing
            std::cerr << "Error in handler '" << name << "': " << e.what() << std::endl;
        }
    }
    
    processedErrors_++;
}

bool ErrorReporter::shouldProcess(std::shared_ptr<ErrorContext> context) {
    std::shared_lock<std::shared_mutex> lock(handlersMutex_);
    
    for (const auto& [name, filter] : filters_) {
        try {
            if (!filter(context)) {
                return false;
            }
        } catch (const std::exception& e) {
            // Log filter error but continue processing
            std::cerr << "Error in filter '" << name << "': " << e.what() << std::endl;
        }
    }
    
    return true;
}

void ErrorReporter::updateStatistics(std::shared_ptr<ErrorContext> context) {
    severityStats_[context->getSeverity()]++;
    categoryStats_[context->getCategory()]++;
}

void ErrorReporter::aggregateError(std::shared_ptr<ErrorContext> context) {
    std::lock_guard<std::mutex> lock(aggregationMutex_);
    
    std::string key = getAggregationKey(context);
    auto now = std::chrono::steady_clock::now();
    
    // Check if we need to flush old aggregated errors
    auto it = aggregationTimestamps_.find(key);
    if (it != aggregationTimestamps_.end()) {
        if (now - it->second > aggregationWindow_) {
            // Process aggregated errors
            auto& errors = aggregatedErrors_[key];
            if (!errors.empty()) {
                // Create aggregated context or process individually
                for (auto& error : errors) {
                    // Process aggregated error
                }
                errors.clear();
            }
            aggregationTimestamps_[key] = now;
        }
    } else {
        aggregationTimestamps_[key] = now;
    }
    
    aggregatedErrors_[key].push_back(context);
}

std::string ErrorReporter::getAggregationKey(std::shared_ptr<ErrorContext> context) {
    switch (aggregationStrategy_) {
        case AggregationStrategy::BySeverity:
            return std::string(severityToString(context->getSeverity()));
        case AggregationStrategy::ByCategory:
            return std::string(categoryToString(context->getCategory()));
        case AggregationStrategy::ByCode:
            return std::to_string(context->getErrorCode());
        case AggregationStrategy::ByCorrelation:
            return context->getCorrelationId();
        case AggregationStrategy::ByTimeWindow:
            {
                auto timestamp = context->getTimestamp();
                auto time_t = std::chrono::system_clock::to_time_t(timestamp);
                auto window_seconds = std::chrono::duration_cast<std::chrono::seconds>(aggregationWindow_).count();
                auto window_start = (time_t / window_seconds) * window_seconds;
                return std::to_string(window_start);
            }
        default:
            return context->getErrorId();
    }
}

// ErrorAggregator implementation
ErrorAggregator::ErrorAggregator() = default;

void ErrorAggregator::addError(std::shared_ptr<ErrorContext> context) {
    if (!context) return;

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
    return it != aggregatedErrors_.end() ? it->second : std::vector<std::shared_ptr<ErrorContext>>{};
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
            std::remove_if(errors.begin(), errors.end(),
                [cutoff](const std::shared_ptr<ErrorContext>& context) {
                    return context && context->getTimestamp() < cutoff;
                }),
            errors.end()
        );

        if (errors.empty()) {
            it = aggregatedErrors_.erase(it);
        } else {
            ++it;
        }
    }
}

auto ErrorAggregator::getStatistics() const -> std::unordered_map<std::string, int> {
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

// GlobalErrorHandler implementation
auto GlobalErrorHandler::getInstance() -> GlobalErrorHandler& {
    static GlobalErrorHandler instance;
    return instance;
}

void GlobalErrorHandler::initialize() {
    std::lock_guard<std::mutex> lock(initMutex_);
    if (!initialized_.load()) {
        reporter_ = std::make_unique<ErrorReporter>();
        aggregator_ = std::make_unique<ErrorAggregator>();

        // Add default handler that forwards to aggregator
        reporter_->addHandler("aggregator", [this](std::shared_ptr<ErrorContext> context) {
            aggregator_->addError(context);
        });

        reporter_->start();
        initialized_ = true;
    }
}

void GlobalErrorHandler::shutdown() {
    std::lock_guard<std::mutex> lock(initMutex_);
    if (initialized_.load()) {
        if (reporter_) {
            reporter_->stop();
            reporter_.reset();
        }
        aggregator_.reset();
        initialized_ = false;
    }
}

void GlobalErrorHandler::reportError(std::shared_ptr<ErrorContext> context) {
    if (!initialized_.load()) {
        initialize();
    }

    if (reporter_) {
        reporter_->reportError(context);
    }

    if (globalHandler_) {
        try {
            globalHandler_(context);
        } catch (const std::exception& e) {
            std::cerr << "Error in global handler: " << e.what() << std::endl;
        }
    }
}

auto GlobalErrorHandler::getReporter() -> ErrorReporter& {
    if (!initialized_.load()) {
        initialize();
    }
    return *reporter_;
}

auto GlobalErrorHandler::getAggregator() -> ErrorAggregator& {
    if (!initialized_.load()) {
        initialize();
    }
    return *aggregator_;
}

void GlobalErrorHandler::setGlobalHandler(ErrorHandlerCallback handler) {
    globalHandler_ = std::move(handler);
}

void GlobalErrorHandler::setUnhandledExceptionHandler() {
    std::set_terminate([]() {
        GlobalErrorHandler::getInstance().handleUnhandledException();
        std::abort();
    });
}

void GlobalErrorHandler::handleUnhandledException() {
    try {
        auto context = ErrorContext::create(
            static_cast<int>(ErrorCodeBase::Failed),
            "Unhandled exception occurred"
        );
        context->addTag("unhandled_exception");
        context->setSystemInfo("severity", "fatal");

        reportError(context);
    } catch (...) {
        // Last resort - just log to stderr
        std::cerr << "Fatal: Unhandled exception in error handler" << std::endl;
    }
}

// ThreadLocalErrorHandler implementation
thread_local std::unique_ptr<ThreadLocalErrorHandler> tlsErrorHandler;

ThreadLocalErrorHandler::ThreadLocalErrorHandler()
    : errorCount_(0)
    , startTime_(std::chrono::steady_clock::now()) {
}

ThreadLocalErrorHandler::~ThreadLocalErrorHandler() = default;

void ThreadLocalErrorHandler::setHandler(ErrorHandlerCallback handler) {
    handler_ = std::move(handler);
}

void ThreadLocalErrorHandler::reportError(std::shared_ptr<ErrorContext> context) {
    errorCount_++;

    if (handler_) {
        try {
            handler_(context);
        } catch (const std::exception& e) {
            std::cerr << "Error in thread-local handler: " << e.what() << std::endl;
        }
    }

    // Also report to global handler
    GlobalErrorHandler::getInstance().reportError(context);
}

auto ThreadLocalErrorHandler::getStatistics() const -> std::unordered_map<std::string, int> {
    std::unordered_map<std::string, int> stats;
    stats["thread_error_count"] = errorCount_.load();

    auto duration = std::chrono::steady_clock::now() - startTime_;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    stats["thread_uptime_seconds"] = static_cast<int>(seconds);

    return stats;
}

} // namespace atom::error

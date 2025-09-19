/*
 * error_handler.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Thread-safe error handling system with aggregation and reporting

**************************************************/

#ifndef ATOM_ERROR_HANDLER_HPP
#define ATOM_ERROR_HANDLER_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "error_context.hpp"
#include "error_code.hpp"

namespace atom::error {

/**
 * @brief Error handler callback function type
 */
using ErrorHandlerCallback = std::function<void(std::shared_ptr<ErrorContext>)>;

/**
 * @brief Error filter function type
 */
using ErrorFilter = std::function<bool(std::shared_ptr<ErrorContext>)>;

/**
 * @brief Error aggregation strategy
 */
enum class AggregationStrategy {
    None,           ///< No aggregation
    BySeverity,     ///< Aggregate by severity level
    ByCategory,     ///< Aggregate by error category
    ByCode,         ///< Aggregate by error code
    ByCorrelation,  ///< Aggregate by correlation ID
    ByTimeWindow    ///< Aggregate by time window
};

/**
 * @brief Thread-safe error reporter for collecting and processing errors
 */
class ErrorReporter {
public:
    /**
     * @brief Constructor
     */
    ErrorReporter();
    
    /**
     * @brief Destructor
     */
    ~ErrorReporter();
    
    /**
     * @brief Start the error reporter
     */
    void start();
    
    /**
     * @brief Stop the error reporter
     */
    void stop();
    
    /**
     * @brief Check if the reporter is running
     */
    [[nodiscard]] auto isRunning() const -> bool;
    
    /**
     * @brief Report an error
     */
    void reportError(std::shared_ptr<ErrorContext> context);
    
    /**
     * @brief Add error handler callback
     */
    void addHandler(const std::string& name, ErrorHandlerCallback handler);
    
    /**
     * @brief Remove error handler callback
     */
    void removeHandler(const std::string& name);
    
    /**
     * @brief Add error filter
     */
    void addFilter(const std::string& name, ErrorFilter filter);
    
    /**
     * @brief Remove error filter
     */
    void removeFilter(const std::string& name);
    
    /**
     * @brief Set aggregation strategy
     */
    void setAggregationStrategy(AggregationStrategy strategy);
    
    /**
     * @brief Set aggregation time window (for time-based aggregation)
     */
    void setAggregationWindow(std::chrono::milliseconds window);
    
    /**
     * @brief Get error statistics
     */
    [[nodiscard]] auto getStatistics() const -> std::unordered_map<std::string, int>;
    
    /**
     * @brief Clear error statistics
     */
    void clearStatistics();
    
    /**
     * @brief Set maximum queue size
     */
    void setMaxQueueSize(size_t maxSize);
    
    /**
     * @brief Get current queue size
     */
    [[nodiscard]] auto getQueueSize() const -> size_t;

private:
    void processingLoop();
    void processError(std::shared_ptr<ErrorContext> context);
    bool shouldProcess(std::shared_ptr<ErrorContext> context);
    void updateStatistics(std::shared_ptr<ErrorContext> context);
    void aggregateError(std::shared_ptr<ErrorContext> context);
    std::string getAggregationKey(std::shared_ptr<ErrorContext> context);
    
    // Threading
    std::atomic<bool> running_;
    std::thread processingThread_;
    
    // Error queue
    std::queue<std::shared_ptr<ErrorContext>> errorQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    size_t maxQueueSize_;
    
    // Handlers and filters
    std::unordered_map<std::string, ErrorHandlerCallback> handlers_;
    std::unordered_map<std::string, ErrorFilter> filters_;
    mutable std::shared_mutex handlersMutex_;
    
    // Aggregation
    AggregationStrategy aggregationStrategy_;
    std::chrono::milliseconds aggregationWindow_;
    std::unordered_map<std::string, std::vector<std::shared_ptr<ErrorContext>>> aggregatedErrors_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> aggregationTimestamps_;
    mutable std::mutex aggregationMutex_;
    
    // Statistics
    std::atomic<int> totalErrors_;
    std::atomic<int> processedErrors_;
    std::atomic<int> filteredErrors_;
    std::atomic<int> droppedErrors_;
    std::unordered_map<ErrorSeverity, std::atomic<int>> severityStats_;
    std::unordered_map<ErrorCategory, std::atomic<int>> categoryStats_;
    mutable std::mutex statsMutex_;
};

/**
 * @brief Thread-safe error aggregator for collecting related errors
 */
class ErrorAggregator {
public:
    /**
     * @brief Constructor
     */
    ErrorAggregator();
    
    /**
     * @brief Destructor
     */
    ~ErrorAggregator() = default;
    
    /**
     * @brief Add error to aggregation
     */
    void addError(std::shared_ptr<ErrorContext> context);
    
    /**
     * @brief Get aggregated errors by key
     */
    [[nodiscard]] auto getAggregatedErrors(const std::string& key) const 
        -> std::vector<std::shared_ptr<ErrorContext>>;
    
    /**
     * @brief Get all aggregation keys
     */
    [[nodiscard]] auto getAggregationKeys() const -> std::vector<std::string>;
    
    /**
     * @brief Clear aggregated errors
     */
    void clear();
    
    /**
     * @brief Clear aggregated errors older than specified time
     */
    void clearOlderThan(std::chrono::minutes maxAge);
    
    /**
     * @brief Get aggregation statistics
     */
    [[nodiscard]] auto getStatistics() const -> std::unordered_map<std::string, int>;

private:
    std::unordered_map<std::string, std::vector<std::shared_ptr<ErrorContext>>> aggregatedErrors_;
    mutable std::shared_mutex aggregationMutex_;
};

/**
 * @brief Global error handler singleton
 */
class GlobalErrorHandler {
public:
    /**
     * @brief Get the singleton instance
     */
    static auto getInstance() -> GlobalErrorHandler&;
    
    /**
     * @brief Initialize the global error handler
     */
    void initialize();
    
    /**
     * @brief Shutdown the global error handler
     */
    void shutdown();
    
    /**
     * @brief Report an error globally
     */
    void reportError(std::shared_ptr<ErrorContext> context);
    
    /**
     * @brief Get the error reporter
     */
    [[nodiscard]] auto getReporter() -> ErrorReporter&;
    
    /**
     * @brief Get the error aggregator
     */
    [[nodiscard]] auto getAggregator() -> ErrorAggregator&;
    
    /**
     * @brief Set global error handler callback
     */
    void setGlobalHandler(ErrorHandlerCallback handler);
    
    /**
     * @brief Set unhandled exception handler
     */
    void setUnhandledExceptionHandler();

private:
    GlobalErrorHandler() = default;
    ~GlobalErrorHandler() = default;
    GlobalErrorHandler(const GlobalErrorHandler&) = delete;
    GlobalErrorHandler& operator=(const GlobalErrorHandler&) = delete;
    
    void handleUnhandledException();
    
    std::unique_ptr<ErrorReporter> reporter_;
    std::unique_ptr<ErrorAggregator> aggregator_;
    ErrorHandlerCallback globalHandler_;
    std::atomic<bool> initialized_;
    mutable std::mutex initMutex_;
};

/**
 * @brief RAII helper for thread-local error handling
 */
class ThreadLocalErrorHandler {
public:
    /**
     * @brief Constructor
     */
    ThreadLocalErrorHandler();
    
    /**
     * @brief Destructor
     */
    ~ThreadLocalErrorHandler();
    
    /**
     * @brief Set thread-local error handler
     */
    void setHandler(ErrorHandlerCallback handler);
    
    /**
     * @brief Report error in current thread
     */
    void reportError(std::shared_ptr<ErrorContext> context);
    
    /**
     * @brief Get thread-local error statistics
     */
    [[nodiscard]] auto getStatistics() const -> std::unordered_map<std::string, int>;

private:
    ErrorHandlerCallback handler_;
    std::atomic<int> errorCount_;
    std::chrono::steady_clock::time_point startTime_;
};

/**
 * @brief Macros for convenient error reporting
 */
#define REPORT_ERROR(errorCode, message) \
    do { \
        auto context = CREATE_ERROR_CONTEXT(errorCode, message); \
        atom::error::GlobalErrorHandler::getInstance().reportError(context); \
    } while(0)

#define REPORT_ERROR_WITH_CORRELATION(errorCode, correlationId, message) \
    do { \
        auto context = atom::error::ErrorContext::createWithCorrelation(errorCode, correlationId, message); \
        context->setSystemInfo("file", ATOM_FILE_NAME) \
               ->setSystemInfo("line", std::to_string(ATOM_FILE_LINE)) \
               ->setSystemInfo("function", ATOM_FUNC_NAME); \
        atom::error::GlobalErrorHandler::getInstance().reportError(context); \
    } while(0)

} // namespace atom::error

#endif // ATOM_ERROR_HANDLER_HPP

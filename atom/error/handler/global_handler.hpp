/*
 * global_handler.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Global error handler singleton

**************************************************/

#ifndef ATOM_ERROR_HANDLER_GLOBAL_HPP
#define ATOM_ERROR_HANDLER_GLOBAL_HPP

#include <atomic>
#include <memory>
#include <mutex>

#include "error_aggregator.hpp"
#include "error_reporter.hpp"

namespace atom::error {

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
    std::atomic<bool> initialized_{false};
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
    [[nodiscard]] auto getStatistics() const
        -> std::unordered_map<std::string, int>;

private:
    ErrorHandlerCallback handler_;
    std::atomic<int> errorCount_;
    std::chrono::steady_clock::time_point startTime_;
};

/**
 * @brief Macros for convenient error reporting
 */
#define REPORT_ERROR(errorCode, message)                                     \
    do {                                                                     \
        auto context = CREATE_ERROR_CONTEXT(errorCode, message);             \
        atom::error::GlobalErrorHandler::getInstance().reportError(context); \
    } while (0)

#define REPORT_ERROR_WITH_CORRELATION(errorCode, correlationId, message)     \
    do {                                                                     \
        auto context = atom::error::ErrorContext::createWithCorrelation(     \
            errorCode, correlationId, message);                              \
        context->setSystemInfo("file", ATOM_FILE_NAME);                      \
        context->setSystemInfo("line", std::to_string(ATOM_FILE_LINE));      \
        context->setSystemInfo("function", ATOM_FUNC_NAME);                  \
        atom::error::GlobalErrorHandler::getInstance().reportError(context); \
    } while (0)

}  // namespace atom::error

#endif  // ATOM_ERROR_HANDLER_GLOBAL_HPP

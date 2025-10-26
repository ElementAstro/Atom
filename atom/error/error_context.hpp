/*
 * error_context.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Error context system for capturing additional debugging information

**************************************************/

#ifndef ATOM_ERROR_CONTEXT_HPP
#define ATOM_ERROR_CONTEXT_HPP

#include <any>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../macro.hpp"
#include "error_code.hpp"

namespace atom::error {

/**
 * @brief Unique identifier for error correlation
 */
using ErrorId = std::string;

/**
 * @brief Generate a unique error ID
 */
ErrorId generateErrorId();

/**
 * @brief Error context information for enhanced debugging
 */
class ErrorContext {
public:
    /**
     * @brief Constructor with basic error information
     */
    ErrorContext(int errorCode, std::string message = "");

    /**
     * @brief Copy constructor
     */
    ErrorContext(const ErrorContext& other);

    /**
     * @brief Move constructor
     */
    ErrorContext(ErrorContext&& other) noexcept;

    /**
     * @brief Assignment operator
     */
    ErrorContext& operator=(const ErrorContext& other);

    /**
     * @brief Move assignment operator
     */
    ErrorContext& operator=(ErrorContext&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~ErrorContext() = default;

    // Basic error information
    [[nodiscard]] auto getErrorId() const -> const ErrorId& { return errorId_; }
    [[nodiscard]] auto getErrorCode() const -> int { return errorCode_; }
    [[nodiscard]] auto getMessage() const -> const std::string& {
        return message_;
    }
    [[nodiscard]] auto getTimestamp() const
        -> std::chrono::system_clock::time_point {
        return timestamp_;
    }
    [[nodiscard]] auto getThreadId() const -> std::thread::id {
        return threadId_;
    }

    // Error metadata
    [[nodiscard]] auto getSeverity() const -> ErrorSeverity {
        return metadata_.severity;
    }
    [[nodiscard]] auto getCategory() const -> ErrorCategory {
        return metadata_.category;
    }
    [[nodiscard]] auto getRecoveryStrategy() const -> ErrorRecoveryStrategy {
        return metadata_.recovery;
    }
    [[nodiscard]] auto getMetadata() const -> const ErrorMetadata& {
        return metadata_;
    }

    // Context information
    auto setUserData(const std::string& key, std::any value) -> ErrorContext&;
    [[nodiscard]] auto getUserData(const std::string& key) const -> std::any;
    [[nodiscard]] auto hasUserData(const std::string& key) const -> bool;

    auto setSystemInfo(const std::string& key,
                       std::string value) -> ErrorContext&;
    [[nodiscard]] auto getSystemInfo(const std::string& key) const
        -> std::string;

    auto addTag(const std::string& tag) -> ErrorContext&;
    [[nodiscard]] auto getTags() const -> const std::vector<std::string>&;
    [[nodiscard]] auto hasTag(const std::string& tag) const -> bool;

    // Error correlation
    auto setCorrelationId(const std::string& correlationId) -> ErrorContext&;
    [[nodiscard]] auto getCorrelationId() const -> const std::string&;

    auto setParentErrorId(const ErrorId& parentId) -> ErrorContext&;
    [[nodiscard]] auto getParentErrorId() const -> const ErrorId&;

    auto addChildErrorId(const ErrorId& childId) -> ErrorContext&;
    [[nodiscard]] auto getChildErrorIds() const -> const std::vector<ErrorId>&;

    // Retry information
    auto incrementRetryCount() -> ErrorContext&;
    [[nodiscard]] auto getRetryCount() const -> int;
    auto setMaxRetries(int maxRetries) -> ErrorContext&;
    [[nodiscard]] auto getMaxRetries() const -> int;
    [[nodiscard]] auto canRetry() const -> bool;

    // Stack trace integration
    auto setStackTrace(const std::string& stackTrace) -> ErrorContext&;
    [[nodiscard]] auto getStackTrace() const -> const std::string&;

    // Serialization
    [[nodiscard]] auto toJson() const -> std::string;
    [[nodiscard]] auto toString() const -> std::string;

    // Static factory methods
    static auto create(int errorCode, const std::string& message = "")
        -> std::shared_ptr<ErrorContext>;
    static auto createWithCorrelation(
        int errorCode, const std::string& correlationId,
        const std::string& message = "") -> std::shared_ptr<ErrorContext>;

private:
    ErrorId errorId_;
    int errorCode_;
    std::string message_;
    std::chrono::system_clock::time_point timestamp_;
    std::thread::id threadId_;
    ErrorMetadata metadata_;

    // Context data
    std::unordered_map<std::string, std::any> userData_;
    std::unordered_map<std::string, std::string> systemInfo_;
    std::vector<std::string> tags_;

    // Error correlation
    std::string correlationId_;
    ErrorId parentErrorId_;
    std::vector<ErrorId> childErrorIds_;

    // Retry information
    int retryCount_;
    int maxRetries_;

    // Stack trace
    std::string stackTrace_;

    // Thread safety
    mutable std::mutex mutex_;

    void initializeSystemInfo();
};

/**
 * @brief Error context manager for tracking error contexts across the
 * application
 */
class ErrorContextManager {
public:
    /**
     * @brief Get the singleton instance
     */
    static auto getInstance() -> ErrorContextManager&;

    /**
     * @brief Register an error context
     */
    void registerContext(std::shared_ptr<ErrorContext> context);

    /**
     * @brief Get error context by ID
     */
    [[nodiscard]] auto getContext(const ErrorId& errorId) const
        -> std::shared_ptr<ErrorContext>;

    /**
     * @brief Get all contexts with a specific correlation ID
     */
    [[nodiscard]] auto getContextsByCorrelation(
        const std::string& correlationId) const
        -> std::vector<std::shared_ptr<ErrorContext>>;

    /**
     * @brief Get error context statistics
     */
    [[nodiscard]] auto getStatistics() const
        -> std::unordered_map<std::string, int>;

    /**
     * @brief Clear old error contexts (cleanup)
     */
    void cleanup(std::chrono::minutes maxAge = std::chrono::minutes(60));

    /**
     * @brief Clear all error contexts
     */
    void clear();

private:
    ErrorContextManager() = default;
    ~ErrorContextManager() = default;
    ErrorContextManager(const ErrorContextManager&) = delete;
    ErrorContextManager& operator=(const ErrorContextManager&) = delete;

    std::unordered_map<ErrorId, std::shared_ptr<ErrorContext>> contexts_;
    mutable std::shared_mutex contextsMutex_;
};

/**
 * @brief RAII helper for automatic error context management
 */
class ScopedErrorContext {
public:
    explicit ScopedErrorContext(std::shared_ptr<ErrorContext> context);
    ~ScopedErrorContext();

    [[nodiscard]] auto getContext() const -> std::shared_ptr<ErrorContext>;

private:
    std::shared_ptr<ErrorContext> context_;
};

/**
 * @brief Macro for creating error context with current location
 */
#define CREATE_ERROR_CONTEXT(errorCode, message)                           \
    ([&]() {                                                               \
        auto _ctx = atom::error::ErrorContext::create(errorCode, message); \
        _ctx->setSystemInfo("file", ATOM_FILE_NAME);                       \
        _ctx->setSystemInfo("line", std::to_string(ATOM_FILE_LINE));       \
        _ctx->setSystemInfo("function", ATOM_FUNC_NAME);                   \
        return _ctx;                                                       \
    })()

/**
 * @brief Macro for creating scoped error context
 */
#define SCOPED_ERROR_CONTEXT(errorCode, message)   \
    atom::error::ScopedErrorContext scopedContext( \
        CREATE_ERROR_CONTEXT(errorCode, message))

}  // namespace atom::error

#endif  // ATOM_ERROR_CONTEXT_HPP

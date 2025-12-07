/*
 * error_context.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Error context class for capturing additional debugging information

**************************************************/

#ifndef ATOM_ERROR_CONTEXT_ERROR_CONTEXT_HPP
#define ATOM_ERROR_CONTEXT_ERROR_CONTEXT_HPP

#include <any>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../../macro.hpp"
#include "../core/error_metadata.hpp"

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

}  // namespace atom::error

#endif  // ATOM_ERROR_CONTEXT_ERROR_CONTEXT_HPP

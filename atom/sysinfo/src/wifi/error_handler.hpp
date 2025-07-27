/*
 * error_handler.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - WiFi Error Handling and Logging

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_WIFI_ERROR_HANDLER_HPP
#define ATOM_SYSTEM_MODULE_WIFI_ERROR_HANDLER_HPP

#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"
#include "common.hpp"

namespace atom::system {

/**
 * @brief WiFi-specific exception class
 */
class WiFiException : public std::exception {
public:
    explicit WiFiException(WiFiError error_code, std::string message = "")
        : error_code_(error_code), message_(std::move(message)) {
        full_message_ = "WiFi Error [" + wifiErrorToString(error_code_) + "]";
        if (!message_.empty()) {
            full_message_ += ": " + message_;
        }
    }

    ATOM_NODISCARD auto what() const noexcept -> const char* override {
        return full_message_.c_str();
    }

    ATOM_NODISCARD auto getErrorCode() const noexcept -> WiFiError {
        return error_code_;
    }

    ATOM_NODISCARD auto getMessage() const noexcept -> const std::string& {
        return message_;
    }

private:
    WiFiError error_code_;
    std::string message_;
    std::string full_message_;
};

/**
 * @brief Error context information
 */
struct ErrorContext {
    std::string function_name;
    std::string file_name;
    int line_number;
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> additional_info;
} ATOM_ALIGNAS(32);

/**
 * @brief Error event data
 */
struct ErrorEvent {
    WiFiError error_code;
    std::string error_message;
    ErrorContext context;
    std::chrono::steady_clock::time_point timestamp;
} ATOM_ALIGNAS(32);

/**
 * @brief Error callback function type
 */
using ErrorCallback = std::function<void(const ErrorEvent&)>;

/**
 * @brief Comprehensive error handler for WiFi operations
 */
class WiFiErrorHandler {
public:
    WiFiErrorHandler() = default;
    ~WiFiErrorHandler() = default;

    // Disable copy constructor and assignment
    WiFiErrorHandler(const WiFiErrorHandler&) = delete;
    WiFiErrorHandler& operator=(const WiFiErrorHandler&) = delete;

    // Move constructor and assignment
    WiFiErrorHandler(WiFiErrorHandler&&) noexcept = default;
    WiFiErrorHandler& operator=(WiFiErrorHandler&&) noexcept = default;

    /**
     * @brief Register an error callback
     * @param callback Function to call when errors occur
     */
    void registerErrorCallback(ErrorCallback callback);

    /**
     * @brief Log an error with context
     * @param error_code WiFi error code
     * @param message Error message
     * @param context Error context information
     */
    void logError(WiFiError error_code, const std::string& message, const ErrorContext& context);

    /**
     * @brief Log an error with simplified context
     * @param error_code WiFi error code
     * @param message Error message
     * @param function_name Function where error occurred
     * @param file_name File where error occurred
     * @param line_number Line number where error occurred
     */
    void logError(WiFiError error_code, const std::string& message, 
                  const std::string& function_name, const std::string& file_name, int line_number);

    /**
     * @brief Get recent error events
     * @param duration How far back to retrieve errors
     * @return Vector of error events
     */
    ATOM_NODISCARD auto getRecentErrors(std::chrono::minutes duration = std::chrono::minutes{60}) const -> std::vector<ErrorEvent>;

    /**
     * @brief Get error statistics
     * @param duration Time period to analyze
     * @return Map of error codes to occurrence counts
     */
    ATOM_NODISCARD auto getErrorStatistics(std::chrono::hours duration = std::chrono::hours{24}) const -> std::unordered_map<WiFiError, int>;

    /**
     * @brief Clear error history
     */
    void clearErrorHistory();

    /**
     * @brief Check if a specific error has occurred recently
     * @param error_code Error code to check for
     * @param duration Time period to check
     * @return True if error occurred within the time period
     */
    ATOM_NODISCARD auto hasRecentError(WiFiError error_code, std::chrono::minutes duration = std::chrono::minutes{10}) const -> bool;

    /**
     * @brief Get error rate for a specific error type
     * @param error_code Error code to analyze
     * @param duration Time period to analyze
     * @return Errors per hour
     */
    ATOM_NODISCARD auto getErrorRate(WiFiError error_code, std::chrono::hours duration = std::chrono::hours{1}) const -> double;

    /**
     * @brief Generate error report
     * @param duration Time period to analyze
     * @return Formatted error report
     */
    ATOM_NODISCARD auto generateErrorReport(std::chrono::hours duration = std::chrono::hours{24}) const -> std::string;

private:
    void notifyCallbacks(const ErrorEvent& event);

    mutable std::mutex error_mutex_;
    std::vector<ErrorEvent> error_history_;
    std::vector<ErrorCallback> error_callbacks_;
    
    static constexpr size_t MAX_ERROR_HISTORY = 1000;
};

/**
 * @brief RAII class for automatic error context management
 */
class ErrorContextGuard {
public:
    ErrorContextGuard(const std::string& function_name, const std::string& file_name, int line_number);
    ~ErrorContextGuard();

    // Disable copy constructor and assignment
    ErrorContextGuard(const ErrorContextGuard&) = delete;
    ErrorContextGuard& operator=(const ErrorContextGuard&) = delete;

    // Move constructor and assignment
    ErrorContextGuard(ErrorContextGuard&&) noexcept = default;
    ErrorContextGuard& operator=(ErrorContextGuard&&) noexcept = default;

    /**
     * @brief Add additional context information
     * @param key Context key
     * @param value Context value
     */
    void addContext(const std::string& key, const std::string& value);

    /**
     * @brief Get current error context
     * @return Current error context
     */
    ATOM_NODISCARD auto getContext() const -> const ErrorContext&;

private:
    ErrorContext context_;
};

/**
 * @brief Result wrapper that can contain either a value or an error
 */
template<typename T>
class WiFiResult {
public:
    // Success constructor
    explicit WiFiResult(T value) : value_(std::move(value)), error_code_(WiFiError::SUCCESS) {}

    // Error constructor
    explicit WiFiResult(WiFiError error_code, std::string error_message = "")
        : error_code_(error_code), error_message_(std::move(error_message)) {}

    // Check if result is successful
    ATOM_NODISCARD auto isSuccess() const -> bool {
        return error_code_ == WiFiError::SUCCESS;
    }

    // Check if result is an error
    ATOM_NODISCARD auto isError() const -> bool {
        return error_code_ != WiFiError::SUCCESS;
    }

    // Get the value (throws if error)
    ATOM_NODISCARD auto getValue() const -> const T& {
        if (isError()) {
            throw WiFiException(error_code_, error_message_);
        }
        return value_;
    }

    // Get the value with default fallback
    ATOM_NODISCARD auto getValueOr(const T& default_value) const -> const T& {
        return isSuccess() ? value_ : default_value;
    }

    // Get error code
    ATOM_NODISCARD auto getErrorCode() const -> WiFiError {
        return error_code_;
    }

    // Get error message
    ATOM_NODISCARD auto getErrorMessage() const -> const std::string& {
        return error_message_;
    }

    // Transform the value if successful
    template<typename U>
    ATOM_NODISCARD auto map(std::function<U(const T&)> transform) const -> WiFiResult<U> {
        if (isError()) {
            return WiFiResult<U>(error_code_, error_message_);
        }
        return WiFiResult<U>(transform(value_));
    }

private:
    T value_{};
    WiFiError error_code_;
    std::string error_message_;
};

/**
 * @brief Get global error handler instance
 * @return Reference to global error handler
 */
ATOM_NODISCARD auto getGlobalErrorHandler() -> WiFiErrorHandler&;

/**
 * @brief Initialize global error handling
 * @return True if initialization was successful
 */
ATOM_NODISCARD auto initializeErrorHandling() -> bool;

/**
 * @brief Shutdown global error handling
 */
void shutdownErrorHandling();

} // namespace atom::system

// Convenience macros for error handling
#define WIFI_ERROR_CONTEXT() \
    atom::system::ErrorContextGuard error_guard(__FUNCTION__, __FILE__, __LINE__)

#define WIFI_ADD_CONTEXT(key, value) \
    error_guard.addContext(key, value)

#define WIFI_LOG_ERROR(error_code, message) \
    atom::system::getGlobalErrorHandler().logError(error_code, message, __FUNCTION__, __FILE__, __LINE__)

#define WIFI_THROW_ERROR(error_code, message) \
    throw atom::system::WiFiException(error_code, message)

#define WIFI_RETURN_ERROR(error_code, message) \
    return atom::system::WiFiResult<decltype(return_value)>(error_code, message)

#endif // ATOM_SYSTEM_MODULE_WIFI_ERROR_HANDLER_HPP

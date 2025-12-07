/*
 * error_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Core error type definitions and enumerations

**************************************************/

#ifndef ATOM_ERROR_CORE_ERROR_TYPES_HPP
#define ATOM_ERROR_CORE_ERROR_TYPES_HPP

#include <string_view>

namespace atom::error {

/**
 * @brief Error severity levels for categorizing error importance
 */
enum class ErrorSeverity : int {
    Trace = 0,     ///< Detailed trace information
    Debug = 1,     ///< Debug information
    Info = 2,      ///< Informational messages
    Warning = 3,   ///< Warning conditions
    Error = 4,     ///< Error conditions
    Critical = 5,  ///< Critical conditions
    Fatal = 6      ///< Fatal conditions that may cause termination
};

/**
 * @brief Error categories for grouping related errors
 */
enum class ErrorCategory : int {
    Unknown = 0,        ///< Unknown category
    System = 1,         ///< System-level errors
    Application = 2,    ///< Application-level errors
    Network = 3,        ///< Network-related errors
    IO = 4,             ///< Input/Output errors
    Memory = 5,         ///< Memory-related errors
    Security = 6,       ///< Security-related errors
    Configuration = 7,  ///< Configuration errors
    Validation = 8,     ///< Data validation errors
    Business = 9,       ///< Business logic errors
    External = 10       ///< External service errors
};

/**
 * @brief Error recovery strategies
 */
enum class ErrorRecoveryStrategy : int {
    None = 0,              ///< No recovery possible
    Retry = 1,             ///< Can be retried
    Fallback = 2,          ///< Has fallback mechanism
    UserIntervention = 3,  ///< Requires user intervention
    Restart = 4,           ///< Requires restart
    Ignore = 5             ///< Can be safely ignored
};

/**
 * @brief Base error codes
 */
enum class ErrorCodeBase {
    Success = 0,    ///< Success
    Failed = 1,     ///< Failed
    Cancelled = 2,  ///< Operation cancelled
};

/**
 * @brief Convert error severity to string
 */
constexpr std::string_view severityToString(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::Trace:
            return "TRACE";
        case ErrorSeverity::Debug:
            return "DEBUG";
        case ErrorSeverity::Info:
            return "INFO";
        case ErrorSeverity::Warning:
            return "WARNING";
        case ErrorSeverity::Error:
            return "ERROR";
        case ErrorSeverity::Critical:
            return "CRITICAL";
        case ErrorSeverity::Fatal:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Convert error category to string
 */
constexpr std::string_view categoryToString(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::Unknown:
            return "UNKNOWN";
        case ErrorCategory::System:
            return "SYSTEM";
        case ErrorCategory::Application:
            return "APPLICATION";
        case ErrorCategory::Network:
            return "NETWORK";
        case ErrorCategory::IO:
            return "IO";
        case ErrorCategory::Memory:
            return "MEMORY";
        case ErrorCategory::Security:
            return "SECURITY";
        case ErrorCategory::Configuration:
            return "CONFIGURATION";
        case ErrorCategory::Validation:
            return "VALIDATION";
        case ErrorCategory::Business:
            return "BUSINESS";
        case ErrorCategory::External:
            return "EXTERNAL";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Convert recovery strategy to string
 */
constexpr std::string_view recoveryStrategyToString(
    ErrorRecoveryStrategy strategy) {
    switch (strategy) {
        case ErrorRecoveryStrategy::None:
            return "NONE";
        case ErrorRecoveryStrategy::Retry:
            return "RETRY";
        case ErrorRecoveryStrategy::Fallback:
            return "FALLBACK";
        case ErrorRecoveryStrategy::UserIntervention:
            return "USER_INTERVENTION";
        case ErrorRecoveryStrategy::Restart:
            return "RESTART";
        case ErrorRecoveryStrategy::Ignore:
            return "IGNORE";
        default:
            return "UNKNOWN";
    }
}

}  // namespace atom::error

#endif  // ATOM_ERROR_CORE_ERROR_TYPES_HPP

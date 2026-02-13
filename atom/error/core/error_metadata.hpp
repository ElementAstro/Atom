/*
 * error_metadata.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Error metadata structure and mapper class

**************************************************/

#ifndef ATOM_ERROR_CORE_ERROR_METADATA_HPP
#define ATOM_ERROR_CORE_ERROR_METADATA_HPP

#include <string>
#include <unordered_map>

#include "error_types.hpp"

namespace atom::error {

/**
 * @brief Error metadata structure for enhanced error information
 */
struct ErrorMetadata {
    ErrorSeverity severity = ErrorSeverity::Error;
    ErrorCategory category = ErrorCategory::Unknown;
    ErrorRecoveryStrategy recovery = ErrorRecoveryStrategy::None;
    std::string description;
    std::string solution;
    int retryCount = 0;
    int maxRetries = 3;

    ErrorMetadata() = default;
    ErrorMetadata(ErrorSeverity sev, ErrorCategory cat,
                  ErrorRecoveryStrategy rec, std::string desc = "",
                  std::string sol = "")
        : severity(sev),
          category(cat),
          recovery(rec),
          description(std::move(desc)),
          solution(std::move(sol)) {}
};

/**
 * @brief Error code mapping utilities
 */
class ErrorCodeMapper {
public:
    /**
     * @brief Get error metadata for a specific error code
     */
    static ErrorMetadata getMetadata(int errorCode);

    /**
     * @brief Get human-readable description for error code
     */
    static std::string getDescription(int errorCode);

    /**
     * @brief Get error severity for error code
     */
    static ErrorSeverity getSeverity(int errorCode);

    /**
     * @brief Get error category for error code
     */
    static ErrorCategory getCategory(int errorCode);

    /**
     * @brief Get recovery strategy for error code
     */
    static ErrorRecoveryStrategy getRecoveryStrategy(int errorCode);

    /**
     * @brief Check if error code is recoverable
     */
    static bool isRecoverable(int errorCode);

    /**
     * @brief Check if error code is retryable
     */
    static bool isRetryable(int errorCode);

    /**
     * @brief Register custom error metadata
     */
    static void registerMetadata(int errorCode, const ErrorMetadata& metadata);

    /**
     * @brief Clear all registered metadata
     */
    static void clearMetadata();

private:
    static void initializeErrorMetadata();
    static std::unordered_map<int, ErrorMetadata> errorMetadataMap_;
};

}  // namespace atom::error

#endif  // ATOM_ERROR_CORE_ERROR_METADATA_HPP

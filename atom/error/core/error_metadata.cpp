/*
 * error_metadata.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Implementation of error metadata mapper

**************************************************/

#include "error_metadata.hpp"

#include <mutex>

namespace atom::error {

std::unordered_map<int, ErrorMetadata> ErrorCodeMapper::errorMetadataMap_;

void ErrorCodeMapper::initializeErrorMetadata() {
    static std::once_flag initialized;
    std::call_once(initialized, []() {
        // File errors (100-199)
        errorMetadataMap_[100] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::IO,
            ErrorRecoveryStrategy::Retry, "File not found",
            "Check if the file path is correct and the file exists");

        errorMetadataMap_[101] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::IO,
                          ErrorRecoveryStrategy::Retry, "Cannot open file",
                          "Check file permissions and availability");

        errorMetadataMap_[102] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Security,
            ErrorRecoveryStrategy::UserIntervention, "Access denied",
            "Check file permissions or run with appropriate privileges");

        errorMetadataMap_[103] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::IO,
                          ErrorRecoveryStrategy::Retry, "File read error",
                          "Check file integrity and disk health");

        errorMetadataMap_[104] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::IO,
                          ErrorRecoveryStrategy::Retry, "File write error",
                          "Check disk space and file permissions");

        errorMetadataMap_[105] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Security,
            ErrorRecoveryStrategy::UserIntervention, "Permission denied",
            "Check file permissions or run with elevated privileges");

        errorMetadataMap_[106] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::IO,
                          ErrorRecoveryStrategy::None, "Parse error",
                          "Check file format and content validity");

        errorMetadataMap_[107] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::IO,
                          ErrorRecoveryStrategy::UserIntervention,
                          "Invalid path", "Provide a valid file path");

        errorMetadataMap_[108] =
            ErrorMetadata(ErrorSeverity::Warning, ErrorCategory::IO,
                          ErrorRecoveryStrategy::UserIntervention,
                          "File already exists", "Choose a different filename");

        errorMetadataMap_[111] =
            ErrorMetadata(ErrorSeverity::Critical, ErrorCategory::System,
                          ErrorRecoveryStrategy::UserIntervention, "Disk full",
                          "Free up disk space or use a different location");

        errorMetadataMap_[112] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::System,
                          ErrorRecoveryStrategy::Retry, "Library load error",
                          "Check library path and dependencies");

        errorMetadataMap_[117] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::IO,
                          ErrorRecoveryStrategy::None, "File corrupted",
                          "Restore from backup or recreate the file");

        // Device errors (200-299)
        errorMetadataMap_[201] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::System,
                          ErrorRecoveryStrategy::Retry, "Device not found",
                          "Check device connection and drivers");

        errorMetadataMap_[202] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::System,
                          ErrorRecoveryStrategy::None, "Device not supported",
                          "Use a compatible device or update drivers");

        errorMetadataMap_[203] =
            ErrorMetadata(ErrorSeverity::Warning, ErrorCategory::System,
                          ErrorRecoveryStrategy::Retry, "Device not connected",
                          "Connect the device and try again");

        errorMetadataMap_[206] =
            ErrorMetadata(ErrorSeverity::Warning, ErrorCategory::System,
                          ErrorRecoveryStrategy::Retry, "Device busy",
                          "Wait for the device to become available");

        errorMetadataMap_[230] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::System,
            ErrorRecoveryStrategy::Restart, "Device initialization error",
            "Restart the device or check configuration");

        errorMetadataMap_[234] =
            ErrorMetadata(ErrorSeverity::Critical, ErrorCategory::System,
                          ErrorRecoveryStrategy::UserIntervention,
                          "Device overheating", "Allow device to cool down");

        // Server errors (300-399)
        errorMetadataMap_[300] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Application,
            ErrorRecoveryStrategy::UserIntervention, "Invalid parameters",
            "Check and correct the input parameters");

        errorMetadataMap_[301] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Application,
            ErrorRecoveryStrategy::UserIntervention, "Invalid format",
            "Check the data format requirements");

        errorMetadataMap_[302] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Application,
            ErrorRecoveryStrategy::UserIntervention, "Missing parameters",
            "Provide all required parameters");

        errorMetadataMap_[321] =
            ErrorMetadata(ErrorSeverity::Warning, ErrorCategory::Network,
                          ErrorRecoveryStrategy::Retry, "Request timeout",
                          "Retry the request or check network connection");

        errorMetadataMap_[322] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Security,
            ErrorRecoveryStrategy::UserIntervention, "Authentication failed",
            "Check credentials and try again");

        errorMetadataMap_[324] =
            ErrorMetadata(ErrorSeverity::Warning, ErrorCategory::External,
                          ErrorRecoveryStrategy::Retry, "Server overload",
                          "Wait and retry later when server load decreases");

        // Network errors (400-499)
        errorMetadataMap_[400] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Network,
            ErrorRecoveryStrategy::Retry, "Network connection lost",
            "Check network connectivity and retry");

        errorMetadataMap_[401] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::Network,
                          ErrorRecoveryStrategy::Retry, "Connection refused",
                          "Check if the service is running and accessible");

        errorMetadataMap_[402] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::Network,
                          ErrorRecoveryStrategy::Retry, "DNS lookup failed",
                          "Check DNS settings and hostname");

        errorMetadataMap_[404] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::Security,
                          ErrorRecoveryStrategy::None, "SSL handshake failed",
                          "Check SSL certificates and configuration");

        errorMetadataMap_[407] =
            ErrorMetadata(ErrorSeverity::Critical, ErrorCategory::Network,
                          ErrorRecoveryStrategy::UserIntervention,
                          "Network down", "Check network hardware and cables");

        errorMetadataMap_[411] =
            ErrorMetadata(ErrorSeverity::Warning, ErrorCategory::Network,
                          ErrorRecoveryStrategy::Retry, "Network timeout",
                          "Check network speed and retry with longer timeout");

        // Database errors (500-599)
        errorMetadataMap_[500] = ErrorMetadata(
            ErrorSeverity::Critical, ErrorCategory::External,
            ErrorRecoveryStrategy::Retry, "Database connection failed",
            "Check database server status and connection parameters");

        errorMetadataMap_[501] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::External,
                          ErrorRecoveryStrategy::None, "Query failed",
                          "Check SQL syntax and table structure");

        errorMetadataMap_[502] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::External,
                          ErrorRecoveryStrategy::Retry, "Transaction failed",
                          "Retry the transaction");

        errorMetadataMap_[505] = ErrorMetadata(
            ErrorSeverity::Warning, ErrorCategory::External,
            ErrorRecoveryStrategy::UserIntervention, "Duplicate entry",
            "Use a unique value or update existing record");

        errorMetadataMap_[508] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::External,
                          ErrorRecoveryStrategy::Retry, "Database deadlock",
                          "Transaction will be retried automatically");

        errorMetadataMap_[511] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::External,
                          ErrorRecoveryStrategy::Retry, "Connection timeout",
                          "Check database server and network");

        // Memory errors (600-699)
        errorMetadataMap_[600] = ErrorMetadata(
            ErrorSeverity::Critical, ErrorCategory::Memory,
            ErrorRecoveryStrategy::Restart, "Memory allocation failed",
            "Restart application or free up system memory");

        errorMetadataMap_[601] =
            ErrorMetadata(ErrorSeverity::Fatal, ErrorCategory::Memory,
                          ErrorRecoveryStrategy::Restart, "Out of memory",
                          "Close other applications or restart system");

        errorMetadataMap_[602] =
            ErrorMetadata(ErrorSeverity::Fatal, ErrorCategory::Memory,
                          ErrorRecoveryStrategy::Restart, "Access violation",
                          "Debug the application for memory errors");

        errorMetadataMap_[603] =
            ErrorMetadata(ErrorSeverity::Critical, ErrorCategory::Memory,
                          ErrorRecoveryStrategy::Restart, "Buffer overflow",
                          "Check buffer sizes and input validation");

        errorMetadataMap_[607] =
            ErrorMetadata(ErrorSeverity::Fatal, ErrorCategory::Memory,
                          ErrorRecoveryStrategy::Restart, "Stack overflow",
                          "Reduce recursion depth or increase stack size");

        // User input errors (700-799)
        errorMetadataMap_[700] = ErrorMetadata(
            ErrorSeverity::Warning, ErrorCategory::Validation,
            ErrorRecoveryStrategy::UserIntervention, "Invalid input",
            "Please provide valid input according to the format requirements");

        errorMetadataMap_[701] = ErrorMetadata(
            ErrorSeverity::Warning, ErrorCategory::Validation,
            ErrorRecoveryStrategy::UserIntervention, "Input out of range",
            "Please provide input within the acceptable range");

        errorMetadataMap_[702] = ErrorMetadata(
            ErrorSeverity::Warning, ErrorCategory::Validation,
            ErrorRecoveryStrategy::UserIntervention, "Missing input",
            "Please provide all required input fields");

        errorMetadataMap_[703] =
            ErrorMetadata(ErrorSeverity::Warning, ErrorCategory::Validation,
                          ErrorRecoveryStrategy::UserIntervention,
                          "Format error", "Please check the input format");

        // Configuration errors (800-899)
        errorMetadataMap_[800] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::Configuration,
                          ErrorRecoveryStrategy::UserIntervention,
                          "Missing configuration file",
                          "Create or restore the configuration file");

        errorMetadataMap_[801] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Configuration,
            ErrorRecoveryStrategy::UserIntervention, "Invalid configuration",
            "Check and correct the configuration settings");

        errorMetadataMap_[802] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Configuration,
            ErrorRecoveryStrategy::None, "Configuration parse error",
            "Check configuration file syntax");

        errorMetadataMap_[804] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Configuration,
            ErrorRecoveryStrategy::UserIntervention, "Configuration conflict",
            "Resolve conflicting configuration options");

        // Process errors (900-999)
        errorMetadataMap_[900] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::System,
                          ErrorRecoveryStrategy::Retry, "Process not found",
                          "Check if the process is running");

        errorMetadataMap_[901] =
            ErrorMetadata(ErrorSeverity::Error, ErrorCategory::System,
                          ErrorRecoveryStrategy::Restart, "Process failed",
                          "Restart the process");

        errorMetadataMap_[902] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::System,
            ErrorRecoveryStrategy::Retry, "Thread creation failed",
            "Check system resources and thread limits");

        errorMetadataMap_[905] =
            ErrorMetadata(ErrorSeverity::Critical, ErrorCategory::System,
                          ErrorRecoveryStrategy::Restart, "Deadlock detected",
                          "Restart the application to resolve deadlock");

        errorMetadataMap_[908] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::System,
            ErrorRecoveryStrategy::Retry, "Insufficient resources",
            "Free up system resources and retry");
    });
}

ErrorMetadata ErrorCodeMapper::getMetadata(int errorCode) {
    initializeErrorMetadata();
    if (auto it = errorMetadataMap_.find(errorCode);
        it != errorMetadataMap_.end()) {
        return it->second;
    }
    return {ErrorSeverity::Error, ErrorCategory::Unknown,
            ErrorRecoveryStrategy::None, "Unknown error",
            "Contact support for assistance"};
}

std::string ErrorCodeMapper::getDescription(int errorCode) {
    return getMetadata(errorCode).description;
}

ErrorSeverity ErrorCodeMapper::getSeverity(int errorCode) {
    return getMetadata(errorCode).severity;
}

ErrorCategory ErrorCodeMapper::getCategory(int errorCode) {
    return getMetadata(errorCode).category;
}

ErrorRecoveryStrategy ErrorCodeMapper::getRecoveryStrategy(int errorCode) {
    return getMetadata(errorCode).recovery;
}

bool ErrorCodeMapper::isRecoverable(int errorCode) {
    const auto strategy = getRecoveryStrategy(errorCode);
    return strategy != ErrorRecoveryStrategy::None;
}

bool ErrorCodeMapper::isRetryable(int errorCode) {
    const auto strategy = getRecoveryStrategy(errorCode);
    return strategy == ErrorRecoveryStrategy::Retry;
}

void ErrorCodeMapper::registerMetadata(int errorCode,
                                       const ErrorMetadata& metadata) {
    initializeErrorMetadata();
    errorMetadataMap_.insert_or_assign(errorCode, metadata);
}

void ErrorCodeMapper::clearMetadata() { errorMetadataMap_.clear(); }

}  // namespace atom::error

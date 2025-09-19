/*
 * error_code.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Implementation of error code mapping utilities

**************************************************/

#include "error_code.hpp"
#include <mutex>

namespace atom::error {

std::unordered_map<int, ErrorMetadata> ErrorCodeMapper::errorMetadataMap_;

void ErrorCodeMapper::initializeErrorMetadata() {
    static std::once_flag initialized;
    std::call_once(initialized, []() {
        // File errors
        errorMetadataMap_[100] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::IO, ErrorRecoveryStrategy::Retry,
            "File not found", "Check if the file path is correct and the file exists");
        
        errorMetadataMap_[101] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::IO, ErrorRecoveryStrategy::Retry,
            "Cannot open file", "Check file permissions and availability");
        
        errorMetadataMap_[102] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Security, ErrorRecoveryStrategy::UserIntervention,
            "Access denied", "Check file permissions or run with appropriate privileges");
        
        errorMetadataMap_[103] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::IO, ErrorRecoveryStrategy::Retry,
            "File read error", "Check file integrity and disk health");
        
        errorMetadataMap_[104] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::IO, ErrorRecoveryStrategy::Retry,
            "File write error", "Check disk space and file permissions");
        
        errorMetadataMap_[111] = ErrorMetadata(
            ErrorSeverity::Critical, ErrorCategory::System, ErrorRecoveryStrategy::UserIntervention,
            "Disk full", "Free up disk space or use a different location");
        
        // Device errors
        errorMetadataMap_[201] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::System, ErrorRecoveryStrategy::Retry,
            "Device not found", "Check device connection and drivers");
        
        errorMetadataMap_[202] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::System, ErrorRecoveryStrategy::None,
            "Device not supported", "Use a compatible device or update drivers");
        
        errorMetadataMap_[203] = ErrorMetadata(
            ErrorSeverity::Warning, ErrorCategory::System, ErrorRecoveryStrategy::Retry,
            "Device not connected", "Connect the device and try again");
        
        // Network errors
        errorMetadataMap_[400] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Network, ErrorRecoveryStrategy::Retry,
            "Network connection lost", "Check network connectivity and retry");
        
        errorMetadataMap_[401] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Network, ErrorRecoveryStrategy::Retry,
            "Connection refused", "Check if the service is running and accessible");
        
        errorMetadataMap_[411] = ErrorMetadata(
            ErrorSeverity::Warning, ErrorCategory::Network, ErrorRecoveryStrategy::Retry,
            "Network timeout", "Check network speed and retry with longer timeout");
        
        // Database errors
        errorMetadataMap_[500] = ErrorMetadata(
            ErrorSeverity::Critical, ErrorCategory::External, ErrorRecoveryStrategy::Retry,
            "Database connection failed", "Check database server status and connection parameters");
        
        errorMetadataMap_[508] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::External, ErrorRecoveryStrategy::Retry,
            "Database deadlock", "Transaction will be retried automatically");
        
        // Memory errors
        errorMetadataMap_[600] = ErrorMetadata(
            ErrorSeverity::Critical, ErrorCategory::Memory, ErrorRecoveryStrategy::Restart,
            "Memory allocation failed", "Restart application or free up system memory");
        
        errorMetadataMap_[601] = ErrorMetadata(
            ErrorSeverity::Fatal, ErrorCategory::Memory, ErrorRecoveryStrategy::Restart,
            "Out of memory", "Close other applications or restart system");
        
        errorMetadataMap_[607] = ErrorMetadata(
            ErrorSeverity::Fatal, ErrorCategory::Memory, ErrorRecoveryStrategy::Restart,
            "Stack overflow", "Reduce recursion depth or increase stack size");
        
        // User input errors
        errorMetadataMap_[700] = ErrorMetadata(
            ErrorSeverity::Warning, ErrorCategory::Validation, ErrorRecoveryStrategy::UserIntervention,
            "Invalid input", "Please provide valid input according to the format requirements");
        
        errorMetadataMap_[701] = ErrorMetadata(
            ErrorSeverity::Warning, ErrorCategory::Validation, ErrorRecoveryStrategy::UserIntervention,
            "Input out of range", "Please provide input within the acceptable range");
        
        // Configuration errors
        errorMetadataMap_[800] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Configuration, ErrorRecoveryStrategy::UserIntervention,
            "Missing configuration file", "Create or restore the configuration file");
        
        errorMetadataMap_[801] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Configuration, ErrorRecoveryStrategy::UserIntervention,
            "Invalid configuration", "Check and correct the configuration settings");
        
        // Process errors
        errorMetadataMap_[900] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::System, ErrorRecoveryStrategy::Retry,
            "Process not found", "Check if the process is running");
        
        errorMetadataMap_[905] = ErrorMetadata(
            ErrorSeverity::Critical, ErrorCategory::System, ErrorRecoveryStrategy::Restart,
            "Deadlock detected", "Restart the application to resolve deadlock");
        
        // Server errors
        errorMetadataMap_[300] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Application, ErrorRecoveryStrategy::UserIntervention,
            "Invalid parameters", "Check and correct the input parameters");
        
        errorMetadataMap_[322] = ErrorMetadata(
            ErrorSeverity::Error, ErrorCategory::Security, ErrorRecoveryStrategy::UserIntervention,
            "Authentication failed", "Check credentials and try again");
        
        errorMetadataMap_[324] = ErrorMetadata(
            ErrorSeverity::Warning, ErrorCategory::External, ErrorRecoveryStrategy::Retry,
            "Server overload", "Wait and retry later when server load decreases");
    });
}

ErrorMetadata ErrorCodeMapper::getMetadata(int errorCode) {
    initializeErrorMetadata();
    auto it = errorMetadataMap_.find(errorCode);
    if (it != errorMetadataMap_.end()) {
        return it->second;
    }
    return ErrorMetadata(ErrorSeverity::Error, ErrorCategory::Unknown, ErrorRecoveryStrategy::None,
                        "Unknown error", "Contact support for assistance");
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
    auto strategy = getRecoveryStrategy(errorCode);
    return strategy != ErrorRecoveryStrategy::None;
}

bool ErrorCodeMapper::isRetryable(int errorCode) {
    auto strategy = getRecoveryStrategy(errorCode);
    return strategy == ErrorRecoveryStrategy::Retry;
}

} // namespace atom::error

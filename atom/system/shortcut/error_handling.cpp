#include "error_handling.h"
#include <sstream>
#include <iomanip>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shortcut_detector {

// Thread-local context stack
thread_local std::vector<ErrorContext> ErrorContextManager::contextStack_;

// ErrorContext implementation
std::string ErrorContext::toString() const {
    std::stringstream ss;
    ss << "Function: " << function << ", File: " << file << ":" << line;

    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    ss << ", Time: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");

    if (!additionalInfo.empty()) {
        ss << ", Additional: {";
        bool first = true;
        for (const auto& [key, value] : additionalInfo) {
            if (!first) ss << ", ";
            ss << key << "=" << value;
            first = false;
        }
        ss << "}";
    }

    return ss.str();
}

// ShortcutDetectorException implementation
ShortcutDetectorException::ShortcutDetectorException(const std::string& message,
                                                   ErrorSeverity severity,
                                                   ErrorCategory category,
                                                   const ErrorContext& context)
    : message_(message), severity_(severity), category_(category), context_(context) {}

const char* ShortcutDetectorException::what() const noexcept {
    if (whatMessage_.empty()) {
        whatMessage_ = getDetailedMessage();
    }
    return whatMessage_.c_str();
}

std::string ShortcutDetectorException::getDetailedMessage() const {
    std::stringstream ss;
    ss << "[" << error_utils::severityToString(severity_) << "] "
       << "[" << error_utils::categoryToString(category_) << "] "
       << message_;

    if (!context_.function.empty()) {
        ss << " (in " << context_.function << ")";
    }

    return ss.str();
}

int ShortcutDetectorException::getErrorCode() const {
    return static_cast<int>(category_) * 1000 + static_cast<int>(severity_);
}

// SystemException implementation
SystemException::SystemException(const std::string& message,
                                const ErrorContext& context,
                                int systemErrorCode)
    : ShortcutDetectorException(message, ErrorSeverity::Error, ErrorCategory::System, context),
      systemErrorCode_(systemErrorCode) {}

std::string SystemException::getSystemErrorMessage() const {
    return error_utils::getSystemErrorMessage(systemErrorCode_);
}

// ValidationException implementation
ValidationException::ValidationException(const std::string& message,
                                        const std::string& fieldName,
                                        const std::string& fieldValue,
                                        const ErrorContext& context)
    : ShortcutDetectorException(message, ErrorSeverity::Warning, ErrorCategory::Validation, context),
      fieldName_(fieldName), fieldValue_(fieldValue) {}

// ConfigurationException implementation
ConfigurationException::ConfigurationException(const std::string& message,
                                              const std::string& configKey,
                                              const ErrorContext& context)
    : ShortcutDetectorException(message, ErrorSeverity::Error, ErrorCategory::Configuration, context),
      configKey_(configKey) {}

// PermissionException implementation
PermissionException::PermissionException(const std::string& message,
                                        const std::string& requiredPermission,
                                        const ErrorContext& context)
    : ShortcutDetectorException(message, ErrorSeverity::Error, ErrorCategory::Permission, context),
      requiredPermission_(requiredPermission) {}

// ResourceException implementation
ResourceException::ResourceException(const std::string& message,
                                    const std::string& resourceType,
                                    const std::string& resourceId,
                                    const ErrorContext& context)
    : ShortcutDetectorException(message, ErrorSeverity::Error, ErrorCategory::Resource, context),
      resourceType_(resourceType), resourceId_(resourceId) {}

// ErrorHandler implementation
ErrorHandler::ErrorHandler() : autoRecovery_(true), maxRecoveryAttempts_(3) {}

ErrorHandler::~ErrorHandler() = default;

void ErrorHandler::handleError(const ShortcutDetectorException& error) {
    // Update statistics
    stats_.totalErrors++;
    stats_.lastError = std::chrono::system_clock::now();

    if (error.getSeverity() == ErrorSeverity::Critical ||
        error.getSeverity() == ErrorSeverity::Fatal) {
        stats_.criticalErrors++;
    }

    // Log the error
    std::string logMessage = error_utils::formatErrorMessage(error);

    switch (error.getSeverity()) {
        case ErrorSeverity::Info:
            spdlog::info(logMessage);
            break;
        case ErrorSeverity::Warning:
            spdlog::warn(logMessage);
            break;
        case ErrorSeverity::Error:
            spdlog::error(logMessage);
            break;
        case ErrorSeverity::Critical:
        case ErrorSeverity::Fatal:
            spdlog::critical(logMessage);
            break;
    }

    // Attempt recovery if enabled
    if (autoRecovery_ && attemptRecovery(error)) {
        stats_.recoveredErrors++;
        spdlog::info("Successfully recovered from error: {}", error.getMessage());
    }

    // Call registered callbacks
    auto it = callbacks_.find(error.getSeverity());
    if (it != callbacks_.end()) {
        for (const auto& callback : it->second) {
            try {
                callback(error);
            } catch (const std::exception& e) {
                spdlog::error("Error in error callback: {}", e.what());
            }
        }
    }
}

void ErrorHandler::registerCallback(ErrorSeverity severity, ErrorCallback callback) {
    callbacks_[severity].push_back(callback);
}

void ErrorHandler::registerRecoveryStrategy(std::unique_ptr<ErrorRecoveryStrategy> strategy) {
    recoveryStrategies_.push_back(std::move(strategy));
}

ErrorHandler::ErrorStats ErrorHandler::getErrorStats() const {
    return stats_;
}

void ErrorHandler::clearStats() {
    stats_ = ErrorStats{};
}

bool ErrorHandler::attemptRecovery(const ShortcutDetectorException& error) {
    for (int attempt = 0; attempt < maxRecoveryAttempts_; ++attempt) {
        for (const auto& strategy : recoveryStrategies_) {
            if (strategy->canHandle(error)) {
                try {
                    if (strategy->recover(error)) {
                        spdlog::debug("Recovery successful with strategy: {}",
                                     strategy->getDescription());
                        return true;
                    }
                } catch (const std::exception& e) {
                    spdlog::warn("Recovery strategy failed: {}", e.what());
                }
            }
        }
    }
    return false;
}

// ErrorContextManager implementation
ErrorContextManager::ErrorContextManager(const std::string& function,
                                        const std::string& file,
                                        int line) {
    contextStack_.emplace_back(function, file, line);
}

ErrorContextManager::~ErrorContextManager() {
    if (!contextStack_.empty()) {
        contextStack_.pop_back();
    }
}

void ErrorContextManager::addContext(const std::string& key, const std::string& value) {
    if (!contextStack_.empty()) {
        contextStack_.back().addInfo(key, value);
    }
}

ErrorContext ErrorContextManager::getCurrentContext() {
    if (!contextStack_.empty()) {
        return contextStack_.back();
    }
    return ErrorContext();
}

// Global error handler
static ErrorHandler g_errorHandler;

ErrorHandler& getGlobalErrorHandler() {
    return g_errorHandler;
}

// Utility functions
namespace error_utils {

std::string severityToString(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::Info: return "INFO";
        case ErrorSeverity::Warning: return "WARNING";
        case ErrorSeverity::Error: return "ERROR";
        case ErrorSeverity::Critical: return "CRITICAL";
        case ErrorSeverity::Fatal: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string categoryToString(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::System: return "SYSTEM";
        case ErrorCategory::Validation: return "VALIDATION";
        case ErrorCategory::Configuration: return "CONFIG";
        case ErrorCategory::Network: return "NETWORK";
        case ErrorCategory::Permission: return "PERMISSION";
        case ErrorCategory::Resource: return "RESOURCE";
        case ErrorCategory::Logic: return "LOGIC";
        case ErrorCategory::External: return "EXTERNAL";
        default: return "UNKNOWN";
    }
}

std::string getSystemErrorMessage(int errorCode) {
#ifdef _WIN32
    if (errorCode == 0) return "Success";

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);

    // Remove trailing newlines
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }

    return message;
#else
    return "Error code: " + std::to_string(errorCode);
#endif
}

std::string formatErrorMessage(const ShortcutDetectorException& error) {
    std::stringstream ss;
    ss << error.getDetailedMessage();

    const auto& context = error.getContext();
    if (!context.function.empty()) {
        ss << "\nContext: " << context.toString();
    }

    // Add specific error type information
    if (auto sysError = dynamic_cast<const SystemException*>(&error)) {
        if (sysError->getSystemErrorCode() != 0) {
            ss << "\nSystem Error: " << sysError->getSystemErrorMessage();
        }
    } else if (auto valError = dynamic_cast<const ValidationException*>(&error)) {
        if (!valError->getFieldName().empty()) {
            ss << "\nField: " << valError->getFieldName();
            if (!valError->getFieldValue().empty()) {
                ss << " = " << valError->getFieldValue();
            }
        }
    } else if (auto configError = dynamic_cast<const ConfigurationException*>(&error)) {
        if (!configError->getConfigKey().empty()) {
            ss << "\nConfig Key: " << configError->getConfigKey();
        }
    }

    return ss.str();
}

}  // namespace error_utils

// Concrete recovery strategies
class PermissionRecoveryStrategy : public ErrorRecoveryStrategy {
public:
    bool recover(const ShortcutDetectorException& error) override {
        if (auto permError = dynamic_cast<const PermissionException*>(&error)) {
            // Try to enable debug privileges
#ifdef _WIN32
            HANDLE hToken;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
                TOKEN_PRIVILEGES tp;
                tp.PrivilegeCount = 1;
                tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

                if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid)) {
                    if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL)) {
                        CloseHandle(hToken);
                        return GetLastError() == ERROR_SUCCESS;
                    }
                }
                CloseHandle(hToken);
            }
#endif
        }
        return false;
    }

    std::string getDescription() const override {
        return "Permission Recovery: Attempt to enable required privileges";
    }

    bool canHandle(const ShortcutDetectorException& error) const override {
        return dynamic_cast<const PermissionException*>(&error) != nullptr;
    }
};

class ResourceRecoveryStrategy : public ErrorRecoveryStrategy {
public:
    bool recover(const ShortcutDetectorException& error) override {
        if (auto resError = dynamic_cast<const ResourceException*>(&error)) {
            // Try to free up resources or retry allocation
            if (resError->getResourceType() == "memory") {
                // Force garbage collection or cleanup
                return true; // Simplified - would implement actual cleanup
            } else if (resError->getResourceType() == "handle") {
                // Try to close unused handles
                return true; // Simplified - would implement handle cleanup
            }
        }
        return false;
    }

    std::string getDescription() const override {
        return "Resource Recovery: Attempt to free resources and retry";
    }

    bool canHandle(const ShortcutDetectorException& error) const override {
        return dynamic_cast<const ResourceException*>(&error) != nullptr;
    }
};

class SystemRecoveryStrategy : public ErrorRecoveryStrategy {
public:
    bool recover(const ShortcutDetectorException& error) override {
        if (auto sysError = dynamic_cast<const SystemException*>(&error)) {
            int errorCode = sysError->getSystemErrorCode();

            // Handle specific system errors
            switch (errorCode) {
                case 5: // ERROR_ACCESS_DENIED
                    // Try alternative access methods
                    return true; // Simplified

                case 2: // ERROR_FILE_NOT_FOUND
                    // Try to locate resource in alternative locations
                    return true; // Simplified

                default:
                    return false;
            }
        }
        return false;
    }

    std::string getDescription() const override {
        return "System Recovery: Handle common system errors";
    }

    bool canHandle(const ShortcutDetectorException& error) const override {
        return dynamic_cast<const SystemException*>(&error) != nullptr;
    }
};

// Initialize default recovery strategies
void initializeDefaultRecoveryStrategies() {
    auto& handler = getGlobalErrorHandler();
    handler.registerRecoveryStrategy(std::make_unique<PermissionRecoveryStrategy>());
    handler.registerRecoveryStrategy(std::make_unique<ResourceRecoveryStrategy>());
    handler.registerRecoveryStrategy(std::make_unique<SystemRecoveryStrategy>());
}

}  // namespace shortcut_detector

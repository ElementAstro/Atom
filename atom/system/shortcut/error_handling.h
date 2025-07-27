#pragma once

#include <exception>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>

namespace shortcut_detector {

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity {
    Info,       // Informational message
    Warning,    // Warning that doesn't prevent operation
    Error,      // Error that prevents current operation
    Critical,   // Critical error that may affect system stability
    Fatal       // Fatal error requiring immediate attention
};

/**
 * @brief Error categories for better classification
 */
enum class ErrorCategory {
    System,         // System-level errors (Windows API, etc.)
    Validation,     // Input validation errors
    Configuration,  // Configuration-related errors
    Network,        // Network-related errors
    Permission,     // Permission/access errors
    Resource,       // Resource allocation/management errors
    Logic,          // Logic/programming errors
    External        // External dependency errors
};

/**
 * @brief Error context information
 */
struct ErrorContext {
    std::string function;
    std::string file;
    int line;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> additionalInfo;
    
    ErrorContext(const std::string& func = "", const std::string& f = "", int l = 0)
        : function(func), file(f), line(l), timestamp(std::chrono::system_clock::now()) {}
    
    void addInfo(const std::string& key, const std::string& value) {
        additionalInfo[key] = value;
    }
    
    std::string toString() const;
};

/**
 * @brief Base exception class for shortcut detector
 */
class ShortcutDetectorException : public std::exception {
public:
    ShortcutDetectorException(const std::string& message, 
                             ErrorSeverity severity = ErrorSeverity::Error,
                             ErrorCategory category = ErrorCategory::Logic,
                             const ErrorContext& context = ErrorContext());
    
    virtual ~ShortcutDetectorException() noexcept = default;
    
    const char* what() const noexcept override;
    
    ErrorSeverity getSeverity() const noexcept { return severity_; }
    ErrorCategory getCategory() const noexcept { return category_; }
    const ErrorContext& getContext() const noexcept { return context_; }
    const std::string& getMessage() const noexcept { return message_; }
    
    /**
     * @brief Get detailed error information
     */
    std::string getDetailedMessage() const;
    
    /**
     * @brief Get error code (derived from category and severity)
     */
    int getErrorCode() const;

protected:
    std::string message_;
    ErrorSeverity severity_;
    ErrorCategory category_;
    ErrorContext context_;
    mutable std::string whatMessage_;
};

/**
 * @brief System-related exceptions
 */
class SystemException : public ShortcutDetectorException {
public:
    SystemException(const std::string& message, 
                   const ErrorContext& context = ErrorContext(),
                   int systemErrorCode = 0);
    
    int getSystemErrorCode() const noexcept { return systemErrorCode_; }
    std::string getSystemErrorMessage() const;

private:
    int systemErrorCode_;
};

/**
 * @brief Validation-related exceptions
 */
class ValidationException : public ShortcutDetectorException {
public:
    ValidationException(const std::string& message,
                       const std::string& fieldName = "",
                       const std::string& fieldValue = "",
                       const ErrorContext& context = ErrorContext());
    
    const std::string& getFieldName() const noexcept { return fieldName_; }
    const std::string& getFieldValue() const noexcept { return fieldValue_; }

private:
    std::string fieldName_;
    std::string fieldValue_;
};

/**
 * @brief Configuration-related exceptions
 */
class ConfigurationException : public ShortcutDetectorException {
public:
    ConfigurationException(const std::string& message,
                          const std::string& configKey = "",
                          const ErrorContext& context = ErrorContext());
    
    const std::string& getConfigKey() const noexcept { return configKey_; }

private:
    std::string configKey_;
};

/**
 * @brief Permission-related exceptions
 */
class PermissionException : public ShortcutDetectorException {
public:
    PermissionException(const std::string& message,
                       const std::string& requiredPermission = "",
                       const ErrorContext& context = ErrorContext());
    
    const std::string& getRequiredPermission() const noexcept { return requiredPermission_; }

private:
    std::string requiredPermission_;
};

/**
 * @brief Resource-related exceptions
 */
class ResourceException : public ShortcutDetectorException {
public:
    ResourceException(const std::string& message,
                     const std::string& resourceType = "",
                     const std::string& resourceId = "",
                     const ErrorContext& context = ErrorContext());
    
    const std::string& getResourceType() const noexcept { return resourceType_; }
    const std::string& getResourceId() const noexcept { return resourceId_; }

private:
    std::string resourceType_;
    std::string resourceId_;
};

/**
 * @brief Error recovery strategy
 */
class ErrorRecoveryStrategy {
public:
    virtual ~ErrorRecoveryStrategy() = default;
    
    /**
     * @brief Attempt to recover from the error
     * @return true if recovery was successful, false otherwise
     */
    virtual bool recover(const ShortcutDetectorException& error) = 0;
    
    /**
     * @brief Get description of the recovery strategy
     */
    virtual std::string getDescription() const = 0;
    
    /**
     * @brief Check if this strategy can handle the given error
     */
    virtual bool canHandle(const ShortcutDetectorException& error) const = 0;
};

/**
 * @brief Error handler for logging and recovery
 */
class ErrorHandler {
public:
    using ErrorCallback = std::function<void(const ShortcutDetectorException&)>;
    
    ErrorHandler();
    ~ErrorHandler();
    
    /**
     * @brief Handle an exception
     */
    void handleError(const ShortcutDetectorException& error);
    
    /**
     * @brief Register error callback
     */
    void registerCallback(ErrorSeverity severity, ErrorCallback callback);
    
    /**
     * @brief Register recovery strategy
     */
    void registerRecoveryStrategy(std::unique_ptr<ErrorRecoveryStrategy> strategy);
    
    /**
     * @brief Enable/disable automatic recovery
     */
    void setAutoRecovery(bool enabled) { autoRecovery_ = enabled; }
    
    /**
     * @brief Set maximum recovery attempts
     */
    void setMaxRecoveryAttempts(int maxAttempts) { maxRecoveryAttempts_ = maxAttempts; }
    
    /**
     * @brief Get error statistics
     */
    struct ErrorStats {
        size_t totalErrors = 0;
        size_t recoveredErrors = 0;
        size_t criticalErrors = 0;
        std::chrono::system_clock::time_point lastError;
    };
    
    ErrorStats getErrorStats() const;
    
    /**
     * @brief Clear error statistics
     */
    void clearStats();

private:
    std::unordered_map<ErrorSeverity, std::vector<ErrorCallback>> callbacks_;
    std::vector<std::unique_ptr<ErrorRecoveryStrategy>> recoveryStrategies_;
    bool autoRecovery_;
    int maxRecoveryAttempts_;
    ErrorStats stats_;
    
    bool attemptRecovery(const ShortcutDetectorException& error);
};

/**
 * @brief RAII error context manager
 */
class ErrorContextManager {
public:
    ErrorContextManager(const std::string& function, const std::string& file, int line);
    ~ErrorContextManager();
    
    void addContext(const std::string& key, const std::string& value);
    
    static ErrorContext getCurrentContext();

private:
    static thread_local std::vector<ErrorContext> contextStack_;
};

/**
 * @brief Utility macros for error handling
 */
#define SHORTCUT_ERROR_CONTEXT() \
    shortcut_detector::ErrorContextManager __error_context(__FUNCTION__, __FILE__, __LINE__)

#define SHORTCUT_ADD_CONTEXT(key, value) \
    __error_context.addContext(key, value)

#define SHORTCUT_THROW_SYSTEM(message, errorCode) \
    throw shortcut_detector::SystemException(message, \
        shortcut_detector::ErrorContextManager::getCurrentContext(), errorCode)

#define SHORTCUT_THROW_VALIDATION(message, field, value) \
    throw shortcut_detector::ValidationException(message, field, value, \
        shortcut_detector::ErrorContextManager::getCurrentContext())

#define SHORTCUT_THROW_CONFIG(message, key) \
    throw shortcut_detector::ConfigurationException(message, key, \
        shortcut_detector::ErrorContextManager::getCurrentContext())

#define SHORTCUT_THROW_PERMISSION(message, permission) \
    throw shortcut_detector::PermissionException(message, permission, \
        shortcut_detector::ErrorContextManager::getCurrentContext())

#define SHORTCUT_THROW_RESOURCE(message, type, id) \
    throw shortcut_detector::ResourceException(message, type, id, \
        shortcut_detector::ErrorContextManager::getCurrentContext())

/**
 * @brief Global error handler instance
 */
extern ErrorHandler& getGlobalErrorHandler();

/**
 * @brief Utility functions
 */
namespace error_utils {
    /**
     * @brief Convert error severity to string
     */
    std::string severityToString(ErrorSeverity severity);
    
    /**
     * @brief Convert error category to string
     */
    std::string categoryToString(ErrorCategory category);
    
    /**
     * @brief Get system error message from error code
     */
    std::string getSystemErrorMessage(int errorCode);
    
    /**
     * @brief Format error message with context
     */
    std::string formatErrorMessage(const ShortcutDetectorException& error);
}

}  // namespace shortcut_detector

#pragma once

#include <expected>
#include <string>
#include <system_error>


namespace modern_log {

/**
 * @enum LogError
 * @brief Enumeration of all possible logging error types.
 *
 * This enum defines error codes for various failure scenarios in the logging
 * system, such as configuration errors, file I/O failures, network issues, and
 * serialization problems. It is used for error handling and reporting
 * throughout the logging library.
 */
enum class LogError {
    none = 0,              ///< No error occurred.
    logger_not_found,      ///< Logger instance was not found.
    invalid_config,        ///< Invalid logger configuration.
    file_creation_failed,  ///< Failed to create or open a log file.
    async_init_failed,     ///< Failed to initialize asynchronous logging.
    sink_creation_failed,  ///< Failed to create a log sink (output target).
    permission_denied,     ///< Insufficient permissions for logging operation.
    disk_full,             ///< Disk is full; cannot write log data.
    network_error,         ///< Network error occurred during logging.
    serialization_failed   ///< Failed to serialize log data.
};

/**
 * @class LogErrorCategory
 * @brief Custom error category for logging errors.
 *
 * This class provides a custom std::error_category implementation for LogError,
 * enabling integration with std::error_code and standard error handling
 * mechanisms. It supplies human-readable error messages for each LogError
 * value.
 */
class LogErrorCategory : public std::error_category {
public:
    /**
     * @brief Get the name of the error category.
     * @return The name as a C-string.
     */
    const char* name() const noexcept override { return "modern_log"; }

    /**
     * @brief Get a human-readable error message for a given error value.
     * @param ev The integer value of the error.
     * @return The error message as a std::string.
     */
    std::string message(int ev) const override {
        switch (static_cast<LogError>(ev)) {
            case LogError::none:
                return "No error";
            case LogError::logger_not_found:
                return "Logger not found";
            case LogError::invalid_config:
                return "Invalid configuration";
            case LogError::file_creation_failed:
                return "Failed to create log file";
            case LogError::async_init_failed:
                return "Failed to initialize async logging";
            case LogError::sink_creation_failed:
                return "Failed to create log sink";
            case LogError::permission_denied:
                return "Permission denied";
            case LogError::disk_full:
                return "Disk full";
            case LogError::network_error:
                return "Network error";
            case LogError::serialization_failed:
                return "Serialization failed";
            default:
                return "Unknown error";
        }
    }
};

/**
 * @brief Get the singleton instance of the logging error category.
 *
 * This function returns a reference to the static LogErrorCategory instance,
 * which is used for associating LogError values with std::error_code.
 *
 * @return Reference to the LogErrorCategory instance.
 */
inline const LogErrorCategory& log_error_category() {
    static LogErrorCategory instance;
    return instance;
}

/**
 * @brief Create a std::error_code from a LogError value.
 *
 * This function enables seamless conversion of LogError values to
 * std::error_code, allowing use with standard error handling facilities.
 *
 * @param e The LogError value.
 * @return Corresponding std::error_code.
 */
inline std::error_code make_error_code(LogError e) {
    return {static_cast<int>(e), log_error_category()};
}

/**
 * @brief Alias for result types using std::expected and LogError.
 *
 * This template alias provides a convenient way to represent the result of an
 * operation that may either return a value of type T or a LogError.
 *
 * @tparam T The type of the expected result value.
 */
template <typename T>
using Result = std::expected<T, LogError>;

}  // namespace modern_log

/**
 * @brief Enable std::error_code support for LogError enum.
 *
 * This specialization allows LogError to be used directly with std::error_code
 * and related standard library facilities.
 */
namespace std {
template <>
struct is_error_code_enum<modern_log::LogError> : true_type {};
}  // namespace std

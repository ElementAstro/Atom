/**
 * @file clipboard_error.hpp
 * @brief Error codes and exception classes for clipboard operations.
 */

#ifndef ATOM_SYSTEM_CLIPBOARD_ERROR_HPP
#define ATOM_SYSTEM_CLIPBOARD_ERROR_HPP

#include <format>
#include <stdexcept>
#include <string>
#include <system_error>

namespace clip {

/**
 * @brief Error codes for clipboard operations
 */
enum class ClipboardErrorCode {
    SUCCESS = 0,
    NOT_OPENED,
    ACCESS_DENIED,
    FORMAT_NOT_SUPPORTED,
    INVALID_DATA,
    SYSTEM_ERROR,
    TIMEOUT,
    OUT_OF_MEMORY,
    PLATFORM_SPECIFIC
};

/**
 * @brief Custom error category for clipboard operations
 */
class ClipboardErrorCategory : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override {
        return "clipboard";
    }

    [[nodiscard]] std::string message(int ev) const override {
        switch (static_cast<ClipboardErrorCode>(ev)) {
            case ClipboardErrorCode::SUCCESS:
                return "Success";
            case ClipboardErrorCode::NOT_OPENED:
                return "Clipboard not opened";
            case ClipboardErrorCode::ACCESS_DENIED:
                return "Access denied to clipboard";
            case ClipboardErrorCode::FORMAT_NOT_SUPPORTED:
                return "Format not supported";
            case ClipboardErrorCode::INVALID_DATA:
                return "Invalid data provided";
            case ClipboardErrorCode::SYSTEM_ERROR:
                return "System error occurred";
            case ClipboardErrorCode::TIMEOUT:
                return "Operation timed out";
            case ClipboardErrorCode::OUT_OF_MEMORY:
                return "Out of memory";
            case ClipboardErrorCode::PLATFORM_SPECIFIC:
                return "Platform-specific error";
            default:
                return "Unknown error";
        }
    }
};

/**
 * @brief Get the clipboard error category instance
 */
[[nodiscard]] inline const ClipboardErrorCategory&
clipboard_category() noexcept {
    static const ClipboardErrorCategory instance;
    return instance;
}

/**
 * @brief Create an error_code for clipboard operations
 */
[[nodiscard]] inline std::error_code make_error_code(
    ClipboardErrorCode e) noexcept {
    return {static_cast<int>(e), clipboard_category()};
}

/**
 * @brief Base exception class for clipboard operations
 */
class ClipboardException : public std::runtime_error {
private:
    std::error_code error_code_;

public:
    explicit ClipboardException(ClipboardErrorCode code)
        : std::runtime_error(make_error_code(code).message()),
          error_code_(make_error_code(code)) {}

    explicit ClipboardException(ClipboardErrorCode code,
                                const std::string& message)
        : std::runtime_error(
              std::format("{}: {}", make_error_code(code).message(), message)),
          error_code_(make_error_code(code)) {}

    explicit ClipboardException(const std::error_code& ec)
        : std::runtime_error(ec.message()), error_code_(ec) {}

    explicit ClipboardException(const std::error_code& ec,
                                const std::string& message)
        : std::runtime_error(std::format("{}: {}", ec.message(), message)),
          error_code_(ec) {}

    [[nodiscard]] const std::error_code& code() const noexcept {
        return error_code_;
    }
};

/**
 * @brief Exception for access denied errors
 */
class ClipboardAccessDeniedException : public ClipboardException {
public:
    ClipboardAccessDeniedException()
        : ClipboardException(ClipboardErrorCode::ACCESS_DENIED) {}

    explicit ClipboardAccessDeniedException(const std::string& message)
        : ClipboardException(ClipboardErrorCode::ACCESS_DENIED, message) {}
};

/**
 * @brief Exception for format not supported errors
 */
class ClipboardFormatException : public ClipboardException {
public:
    ClipboardFormatException()
        : ClipboardException(ClipboardErrorCode::FORMAT_NOT_SUPPORTED) {}

    explicit ClipboardFormatException(const std::string& message)
        : ClipboardException(ClipboardErrorCode::FORMAT_NOT_SUPPORTED,
                             message) {}
};

/**
 * @brief Exception for timeout errors
 */
class ClipboardTimeoutException : public ClipboardException {
public:
    ClipboardTimeoutException()
        : ClipboardException(ClipboardErrorCode::TIMEOUT) {}

    explicit ClipboardTimeoutException(const std::string& message)
        : ClipboardException(ClipboardErrorCode::TIMEOUT, message) {}
};

/**
 * @brief Exception for system errors
 */
class ClipboardSystemException : public ClipboardException {
public:
    ClipboardSystemException()
        : ClipboardException(ClipboardErrorCode::SYSTEM_ERROR) {}

    explicit ClipboardSystemException(const std::string& message)
        : ClipboardException(ClipboardErrorCode::SYSTEM_ERROR, message) {}

    explicit ClipboardSystemException(const std::error_code& ec)
        : ClipboardException(ec) {}

    explicit ClipboardSystemException(const std::error_code& ec,
                                      const std::string& message)
        : ClipboardException(ec, message) {}
};

}  // namespace clip

// Include clipboard_utils.hpp for backward compatibility (ScopeGuard was here)
#include "clipboard_utils.hpp"

#endif  // ATOM_SYSTEM_CLIPBOARD_ERROR_HPP

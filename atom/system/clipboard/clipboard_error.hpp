#pragma once

#include <format>
#include <string>
#include <system_error>

namespace clip {

/**
 * @brief Error categories for clipboard operations
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

}  // namespace clip

// Make ClipboardErrorCode work with std::error_code
namespace std {
template <>
struct is_error_code_enum<clip::ClipboardErrorCode> : true_type {};
}  // namespace std

// Re-open clip namespace for subsequent definitions
namespace clip {

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

/**
 * @brief Scope guard for exception safety
 */
template <typename F>
class ScopeGuard {
private:
    F function_;
    bool should_execute_;

public:
    explicit ScopeGuard(F&& f) noexcept
        : function_(std::forward<F>(f)), should_execute_(true) {}

    ~ScopeGuard() noexcept {
        if (should_execute_) {
            try {
                function_();
            } catch (...) {
                // Suppress exceptions in destructor
            }
        }
    }

    void dismiss() noexcept { should_execute_ = false; }

    // Non-copyable, movable
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    ScopeGuard(ScopeGuard&& other) noexcept
        : function_(std::move(other.function_)),
          should_execute_(other.should_execute_) {
        other.should_execute_ = false;
    }

    ScopeGuard& operator=(ScopeGuard&& other) noexcept {
        if (this != &other) {
            function_ = std::move(other.function_);
            should_execute_ = other.should_execute_;
            other.should_execute_ = false;
        }
        return *this;
    }
};

/**
 * @brief Helper function to create a scope guard
 */
template <typename F>
[[nodiscard]] auto make_scope_guard(F&& f) noexcept {
    return ScopeGuard<std::decay_t<F>>(std::forward<F>(f));
}

}  // namespace clip
#ifndef ATOM_SECRET_RESULT_HPP
#define ATOM_SECRET_RESULT_HPP

#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <type_traits>
#include <cstdint>

namespace atom::secret {

/**
 * @brief Error codes for better error categorization and handling.
 */
enum class ErrorCode : std::uint32_t {
    Success = 0,
    // Generic errors
    InvalidArgument = 1000,
    InvalidState = 1001,
    OutOfMemory = 1002,

    // Encryption errors
    EncryptionFailed = 2000,
    DecryptionFailed = 2001,
    KeyDerivationFailed = 2002,
    InvalidKey = 2003,
    InvalidIV = 2004,

    // Storage errors
    StorageNotFound = 3000,
    StorageAccessDenied = 3001,
    StorageCorrupted = 3002,
    StorageUnavailable = 3003,

    // Password manager errors
    EntryNotFound = 4000,
    EntryAlreadyExists = 4001,
    InvalidMasterPassword = 4002,
    SerializationFailed = 4003,

    // System errors
    PlatformError = 5000,
    NetworkError = 5001,
    FileSystemError = 5002
};

/**
 * @brief Structured error information with code and message.
 */
struct Error {
    ErrorCode code;
    std::string message;

    Error(ErrorCode c, std::string_view msg) : code(c), message(msg) {}
    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}

    // Implicit conversion from ErrorCode for convenience
    Error(ErrorCode c) : code(c), message(getDefaultMessage(c)) {}

    static std::string getDefaultMessage(ErrorCode code);
};

/**
 * @brief Template for operation results, alternative to exceptions.
 * Enhanced with error codes, chaining operations, and better performance.
 * @tparam T The type of the successful result value.
 */

// Primary template
template <typename T>
class Result {
private:
    std::variant<T, Error> data;

    // Private constructor for error creation
    explicit Result(Error&& error) : data(std::move(error)) {}

public:
    // Success constructors
    explicit Result(const T& value) : data(value) {}
    explicit Result(T&& value) noexcept : data(std::move(value)) {}

    // Error factory methods
    static Result<T> Failure(ErrorCode code, std::string_view message) {
        return Result<T>(::atom::secret::Error(code, message));
    }

    static Result<T> Failure(ErrorCode code, std::string message) {
        return Result<T>(::atom::secret::Error(code, std::move(message)));
    }

    static Result<T> Failure(ErrorCode code) {
        return Result<T>(::atom::secret::Error(code));
    }

    // Legacy string error for backward compatibility
    static Result<T> Failure(std::string_view message) {
        return Result<T>(::atom::secret::Error(ErrorCode::InvalidState, message));
    }

    // State checking
    bool isSuccess() const noexcept { return std::holds_alternative<T>(data); }
    bool isError() const noexcept { return std::holds_alternative<::atom::secret::Error>(data); }
    explicit operator bool() const noexcept { return isSuccess(); }

    // Value access
    const T& value() const& {
        if (isError()) {
            const auto& err = std::get<::atom::secret::Error>(data);
            throw std::runtime_error("Attempted to access value of an error Result: " + err.message);
        }
        return std::get<T>(data);
    }

    T&& value() && {
        if (isError()) {
            const auto& err = std::get<::atom::secret::Error>(data);
            throw std::runtime_error("Attempted to access value of an error Result: " + err.message);
        }
        return std::move(std::get<T>(data));
    }

    // Safe value access with default
    T valueOr(const T& defaultValue) const& {
        return isSuccess() ? std::get<T>(data) : defaultValue;
    }

    T valueOr(T&& defaultValue) && {
        return isSuccess() ? std::move(std::get<T>(data)) : std::move(defaultValue);
    }

    // Error access
    const ::atom::secret::Error& error() const {
        if (isSuccess())
            throw std::runtime_error("Attempted to access error of a success Result.");
        return std::get<::atom::secret::Error>(data);
    }

    // Legacy string error access for backward compatibility
    std::string errorMessage() const {
        return isError() ? error().message : "";
    }

    ErrorCode errorCode() const {
        return isError() ? error().code : ErrorCode::Success;
    }

    // Chaining operations
    template<typename F>
    auto map(F&& func) const& -> Result<std::invoke_result_t<F, const T&>> {
        using ReturnType = std::invoke_result_t<F, const T&>;
        if (isError()) {
            return Result<ReturnType>::Failure(error().code, error().message);
        }
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                func(value());
                return Result<ReturnType>();
            } else {
                return Result<ReturnType>(func(value()));
            }
        } catch (const std::exception& e) {
            return Result<ReturnType>::Failure(ErrorCode::InvalidState, e.what());
        }
    }

    template<typename F>
    auto map(F&& func) && -> Result<std::invoke_result_t<F, T&&>> {
        using ReturnType = std::invoke_result_t<F, T&&>;
        if (isError()) {
            return Result<ReturnType>::Failure(error().code, error().message);
        }
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                func(std::move(value()));
                return Result<ReturnType>();
            } else {
                return Result<ReturnType>(func(std::move(value())));
            }
        } catch (const std::exception& e) {
            return Result<ReturnType>::Failure(ErrorCode::InvalidState, e.what());
        }
    }

    template<typename F>
    auto flatMap(F&& func) const& -> std::invoke_result_t<F, const T&> {
        if (isError()) {
            using ReturnType = std::invoke_result_t<F, const T&>;
            return ReturnType::Failure(error().code, error().message);
        }
        try {
            return func(value());
        } catch (const std::exception& e) {
            using ReturnType = std::invoke_result_t<F, const T&>;
            return ReturnType::Failure(ErrorCode::InvalidState, e.what());
        }
    }

    template<typename F>
    auto flatMap(F&& func) && -> std::invoke_result_t<F, T&&> {
        if (isError()) {
            using ReturnType = std::invoke_result_t<F, T&&>;
            return ReturnType::Failure(error().code, error().message);
        }
        try {
            return func(std::move(value()));
        } catch (const std::exception& e) {
            using ReturnType = std::invoke_result_t<F, T&&>;
            return ReturnType::Failure(ErrorCode::InvalidState, e.what());
        }
    }

    // Error handling
    template<typename F>
    Result<T> orElse(F&& func) const& {
        if (isSuccess()) {
            return *this;
        }
        try {
            return func(error());
        } catch (const std::exception& e) {
            return Result<T>::Failure(ErrorCode::InvalidState, e.what());
        }
    }

    template<typename F>
    Result<T> orElse(F&& func) && {
        if (isSuccess()) {
            return std::move(*this);
        }
        try {
            return func(error());
        } catch (const std::exception& e) {
            return Result<T>::Failure(ErrorCode::InvalidState, e.what());
        }
    }
};

// Specialization for void
template <>
class Result<void> {
private:
    std::variant<std::monostate, ::atom::secret::Error> data;

    // Private constructor for error creation
    explicit Result(::atom::secret::Error&& error) : data(std::move(error)) {}

public:
    // Success constructors
    Result() : data(std::monostate{}) {}
    explicit Result(std::monostate) : data(std::monostate{}) {}

    // Error factory methods
    static Result<void> Failure(ErrorCode code, std::string_view message) {
        return Result<void>(::atom::secret::Error(code, message));
    }

    static Result<void> Failure(ErrorCode code, std::string message) {
        return Result<void>(::atom::secret::Error(code, std::move(message)));
    }

    static Result<void> Failure(ErrorCode code) {
        return Result<void>(::atom::secret::Error(code));
    }

    // Legacy string error for backward compatibility
    static Result<void> Failure(std::string_view message) {
        return Result<void>(::atom::secret::Error(ErrorCode::InvalidState, message));
    }

    // State checking
    bool isSuccess() const noexcept { return std::holds_alternative<std::monostate>(data); }
    bool isError() const noexcept { return std::holds_alternative<::atom::secret::Error>(data); }
    explicit operator bool() const noexcept { return isSuccess(); }

    // Value access
    void value() const {
        if (isError()) {
            const auto& err = std::get<::atom::secret::Error>(data);
            throw std::runtime_error("Attempted to access value of an error Result: " + err.message);
        }
    }

    // Error access
    const ::atom::secret::Error& error() const {
        if (isSuccess())
            throw std::runtime_error("Attempted to access error of a success Result.");
        return std::get<::atom::secret::Error>(data);
    }

    // Legacy string error access for backward compatibility
    std::string errorMessage() const {
        return isError() ? error().message : "";
    }

    ErrorCode errorCode() const {
        return isError() ? error().code : ErrorCode::Success;
    }

    // Chaining operations for void
    template<typename F>
    auto map(F&& func) const -> Result<std::invoke_result_t<F>> {
        using ReturnType = std::invoke_result_t<F>;
        if (isError()) {
            return Result<ReturnType>::Failure(error().code, error().message);
        }
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                func();
                return Result<ReturnType>();
            } else {
                return Result<ReturnType>(func());
            }
        } catch (const std::exception& e) {
            return Result<ReturnType>::Failure(ErrorCode::InvalidState, e.what());
        }
    }

    template<typename F>
    auto flatMap(F&& func) const -> std::invoke_result_t<F> {
        if (isError()) {
            using ReturnType = std::invoke_result_t<F>;
            return ReturnType::Failure(error().code, error().message);
        }
        try {
            return func();
        } catch (const std::exception& e) {
            using ReturnType = std::invoke_result_t<F>;
            return ReturnType::Failure(ErrorCode::InvalidState, e.what());
        }
    }

    // Error handling
    template<typename F>
    Result<void> orElse(F&& func) const {
        if (isSuccess()) {
            return *this;
        }
        try {
            return func(error());
        } catch (const std::exception& e) {
            Result<void> result;
            result.data = ::atom::secret::Error(ErrorCode::InvalidState, std::string(e.what()));
            return result;
        }
    }
};

// Implementation of Error::getDefaultMessage
inline std::string Error::getDefaultMessage(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success: return "Success";
        case ErrorCode::InvalidArgument: return "Invalid argument";
        case ErrorCode::InvalidState: return "Invalid state";
        case ErrorCode::OutOfMemory: return "Out of memory";
        case ErrorCode::EncryptionFailed: return "Encryption failed";
        case ErrorCode::DecryptionFailed: return "Decryption failed";
        case ErrorCode::KeyDerivationFailed: return "Key derivation failed";
        case ErrorCode::InvalidKey: return "Invalid key";
        case ErrorCode::InvalidIV: return "Invalid initialization vector";
        case ErrorCode::StorageNotFound: return "Storage not found";
        case ErrorCode::StorageAccessDenied: return "Storage access denied";
        case ErrorCode::StorageCorrupted: return "Storage corrupted";
        case ErrorCode::StorageUnavailable: return "Storage unavailable";
        case ErrorCode::EntryNotFound: return "Entry not found";
        case ErrorCode::EntryAlreadyExists: return "Entry already exists";
        case ErrorCode::InvalidMasterPassword: return "Invalid master password";
        case ErrorCode::SerializationFailed: return "Serialization failed";
        case ErrorCode::PlatformError: return "Platform error";
        case ErrorCode::NetworkError: return "Network error";
        case ErrorCode::FileSystemError: return "File system error";
        default: return "Unknown error";
    }
}

}  // namespace atom::secret

#endif  // ATOM_SECRET_RESULT_HPP

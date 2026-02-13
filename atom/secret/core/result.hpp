#ifndef ATOM_SECRET_CORE_RESULT_HPP
#define ATOM_SECRET_CORE_RESULT_HPP

#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include "error_codes.hpp"

namespace atom::secret {

/**
 * @brief Error wrapper containing error code and optional message.
 */
struct Error {
    ErrorCode code;       ///< The error code.
    std::string message;  ///< Optional detailed error message.

    /**
     * @brief Constructs an Error with just an error code.
     * @param c The error code.
     */
    explicit Error(ErrorCode c) : code(c), message(errorCodeToString(c)) {}

    /**
     * @brief Constructs an Error with an error code and custom message.
     * @param c The error code.
     * @param msg Custom error message.
     */
    Error(ErrorCode c, const std::string& msg) : code(c), message(msg) {}

    /**
     * @brief Constructs an Error with an error code and custom message (move).
     * @param c The error code.
     * @param msg Custom error message.
     */
    Error(ErrorCode c, std::string&& msg) : code(c), message(std::move(msg)) {}

    /**
     * @brief Constructs an Error with just a message (uses Unknown error code).
     * @param msg The error message.
     */
    explicit Error(const std::string& msg)
        : code(ErrorCode::Unknown), message(msg) {}

    /**
     * @brief Constructs an Error with just a message (move, uses Unknown error
     * code).
     * @param msg The error message.
     */
    explicit Error(std::string&& msg)
        : code(ErrorCode::Unknown), message(std::move(msg)) {}
};

/**
 * @brief Template for operation results, alternative to exceptions.
 * @tparam T The type of the successful result value.
 */
template <typename T>
class Result {
private:
    std::variant<T, Error>
        data;  ///< Holds either the success value or an error.

public:
    /**
     * @brief Constructs a Result with a success value (copy).
     * @param value The success value.
     */
    explicit Result(const T& value) : data(value) {}

    /**
     * @brief Constructs a Result with a success value (move).
     * @param value The success value (rvalue).
     */
    explicit Result(T&& value) noexcept : data(std::move(value)) {}

    /**
     * @brief Constructs a Result with an error.
     * @param error The error.
     */
    explicit Result(const Error& error) : data(error) {}

    /**
     * @brief Constructs a Result with an error (move).
     * @param error The error.
     */
    explicit Result(Error&& error) : data(std::move(error)) {}

    // Static factory methods for clarity

    /**
     * @brief Creates a successful Result.
     * @param value The success value.
     * @return A Result containing the success value.
     */
    static Result success(const T& value) { return Result(value); }

    /**
     * @brief Creates a successful Result (move version).
     * @param value The success value.
     * @return A Result containing the success value.
     */
    static Result success(T&& value) { return Result(std::move(value)); }

    /**
     * @brief Creates an error Result with error code.
     * @param code The error code.
     * @return A Result containing the error.
     */
    static Result error(ErrorCode code) { return Result(Error(code)); }

    /**
     * @brief Creates an error Result with error code and message.
     * @param code The error code.
     * @param message The error message.
     * @return A Result containing the error.
     */
    static Result error(ErrorCode code, const std::string& message) {
        return Result(Error(code, message));
    }

    /**
     * @brief Creates an error Result with just a message.
     * @param message The error message.
     * @return A Result containing the error.
     */
    static Result error(const std::string& message) {
        return Result(Error(message));
    }

    /**
     * @brief Checks if the result represents success.
     * @return True if successful, false otherwise.
     */
    bool isSuccess() const noexcept { return std::holds_alternative<T>(data); }

    /**
     * @brief Checks if the result represents an error.
     * @return True if it's an error, false otherwise.
     */
    bool isError() const noexcept {
        return std::holds_alternative<Error>(data);
    }

    /**
     * @brief Implicit conversion to bool (true if success).
     * @return True if successful.
     */
    explicit operator bool() const noexcept { return isSuccess(); }

    /**
     * @brief Gets the success value (const lvalue ref).
     * @return A const reference to the success value.
     * @throws std::runtime_error if the result is an error.
     */
    const T& value() const& {
        if (isError()) {
            const auto& err = std::get<Error>(data);
            throw std::runtime_error(
                "Attempted to access value of an error Result: " + err.message);
        }
        return std::get<T>(data);
    }

    /**
     * @brief Gets the success value (lvalue ref).
     * @return A reference to the success value.
     * @throws std::runtime_error if the result is an error.
     */
    T& value() & {
        if (isError()) {
            const auto& err = std::get<Error>(data);
            throw std::runtime_error(
                "Attempted to access value of an error Result: " + err.message);
        }
        return std::get<T>(data);
    }

    /**
     * @brief Gets the success value (rvalue ref).
     * @return An rvalue reference to the success value.
     * @throws std::runtime_error if the result is an error.
     */
    T&& value() && {
        if (isError()) {
            const auto& err = std::get<Error>(data);
            throw std::runtime_error(
                "Attempted to access value of an error Result: " + err.message);
        }
        return std::move(std::get<T>(data));
    }

    /**
     * @brief Gets the value or a default if error.
     * @param defaultValue The default value to return on error.
     * @return The value or default.
     */
    T valueOr(T&& defaultValue) const& {
        if (isSuccess()) {
            return std::get<T>(data);
        }
        return std::forward<T>(defaultValue);
    }

    /**
     * @brief Gets the value or a default if error (move version).
     * @param defaultValue The default value to return on error.
     * @return The value or default.
     */
    T valueOr(T&& defaultValue) && {
        if (isSuccess()) {
            return std::move(std::get<T>(data));
        }
        return std::forward<T>(defaultValue);
    }

    /**
     * @brief Gets the error.
     * @return A const reference to the error.
     * @throws std::runtime_error if the result is successful.
     */
    const Error& error() const {
        if (isSuccess()) {
            throw std::runtime_error(
                "Attempted to access error of a success Result.");
        }
        return std::get<Error>(data);
    }

    /**
     * @brief Gets the error code.
     * @return The error code or Success if not an error.
     */
    ErrorCode errorCode() const noexcept {
        if (isError()) {
            return std::get<Error>(data).code;
        }
        return ErrorCode::Success;
    }

    /**
     * @brief Gets the error message.
     * @return The error message or empty string if not an error.
     */
    std::string errorMessage() const {
        if (isError()) {
            return std::get<Error>(data).message;
        }
        return "";
    }

    /**
     * @brief Maps the success value to a new type.
     * @tparam U The new type.
     * @tparam F The mapping function type.
     * @param func The mapping function.
     * @return A new Result with the mapped value or the original error.
     */
    template <typename U, typename F>
    Result<U> map(F&& func) const& {
        if (isSuccess()) {
            return Result<U>::success(func(std::get<T>(data)));
        }
        return Result<U>(std::get<Error>(data));
    }

    /**
     * @brief Maps the success value to a new type (move version).
     * @tparam U The new type.
     * @tparam F The mapping function type.
     * @param func The mapping function.
     * @return A new Result with the mapped value or the original error.
     */
    template <typename U, typename F>
    Result<U> map(F&& func) && {
        if (isSuccess()) {
            return Result<U>::success(func(std::move(std::get<T>(data))));
        }
        return Result<U>(std::move(std::get<Error>(data)));
    }

    /**
     * @brief Chains another Result-returning operation.
     * @tparam U The new type.
     * @tparam F The chaining function type.
     * @param func The chaining function.
     * @return The result of the chained operation or the original error.
     */
    template <typename U, typename F>
    Result<U> andThen(F&& func) const& {
        if (isSuccess()) {
            return func(std::get<T>(data));
        }
        return Result<U>(std::get<Error>(data));
    }

    /**
     * @brief Chains another Result-returning operation (move version).
     * @tparam U The new type.
     * @tparam F The chaining function type.
     * @param func The chaining function.
     * @return The result of the chained operation or the original error.
     */
    template <typename U, typename F>
    Result<U> andThen(F&& func) && {
        if (isSuccess()) {
            return func(std::move(std::get<T>(data)));
        }
        return Result<U>(std::move(std::get<Error>(data)));
    }
};

/**
 * @brief Specialization for void Result (operation with no return value).
 */
template <>
class Result<void> {
private:
    std::optional<Error> error_;

public:
    /**
     * @brief Constructs a successful void Result.
     */
    Result() : error_(std::nullopt) {}

    /**
     * @brief Constructs an error void Result.
     * @param error The error.
     */
    explicit Result(const Error& error) : error_(error) {}

    /**
     * @brief Constructs an error void Result (move).
     * @param error The error.
     */
    explicit Result(Error&& error) : error_(std::move(error)) {}

    /**
     * @brief Creates a successful void Result.
     * @return A successful Result.
     */
    static Result success() { return Result(); }

    /**
     * @brief Creates an error void Result.
     * @param code The error code.
     * @return An error Result.
     */
    static Result error(ErrorCode code) { return Result(Error(code)); }

    /**
     * @brief Creates an error void Result with message.
     * @param code The error code.
     * @param message The error message.
     * @return An error Result.
     */
    static Result error(ErrorCode code, const std::string& message) {
        return Result(Error(code, message));
    }

    /**
     * @brief Creates an error void Result with just message.
     * @param message The error message.
     * @return An error Result.
     */
    static Result error(const std::string& message) {
        return Result(Error(message));
    }

    /**
     * @brief Checks if the result represents success.
     * @return True if successful.
     */
    bool isSuccess() const noexcept { return !error_.has_value(); }

    /**
     * @brief Checks if the result represents an error.
     * @return True if error.
     */
    bool isError() const noexcept { return error_.has_value(); }

    /**
     * @brief Implicit conversion to bool.
     * @return True if successful.
     */
    explicit operator bool() const noexcept { return isSuccess(); }

    /**
     * @brief Gets the error.
     * @return The error.
     * @throws std::runtime_error if successful.
     */
    const Error& error() const {
        if (isSuccess()) {
            throw std::runtime_error(
                "Attempted to access error of a success Result.");
        }
        return *error_;
    }

    /**
     * @brief Gets the error code.
     * @return The error code or Success.
     */
    ErrorCode errorCode() const noexcept {
        if (error_) {
            return error_->code;
        }
        return ErrorCode::Success;
    }

    /**
     * @brief Gets the error message.
     * @return The error message or empty string.
     */
    std::string errorMessage() const {
        if (error_) {
            return error_->message;
        }
        return "";
    }
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_CORE_RESULT_HPP

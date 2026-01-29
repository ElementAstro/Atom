#ifndef ATOM_SECRET_RESULT_HPP
#define ATOM_SECRET_RESULT_HPP

#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

namespace atom::secret {

// Error wrapper to distinguish from success values
struct Error {
    std::string message;
    explicit Error(const std::string& msg) : message(msg) {}
    explicit Error(std::string&& msg) : message(std::move(msg)) {}
};

}  // namespace atom::secret

namespace atom::secret {

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
     * @brief Constructs a Result with an error message.
     * @param error The error message string.
     */
    explicit Result(const Error& error) : data(error) {}

    /**
     * @brief Constructs a Result with an error message (move).
     * @param error The error message string.
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
     * @brief Creates an error Result.
     * @param error The error message.
     * @return A Result containing the error.
     */
    static Result error(const std::string& error) {
        return Result(Error(error));
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
     * @brief Gets the success value (const lvalue ref).
     * @return A const reference to the success value.
     * @throws std::runtime_error if the result is an error.
     */
    const T& value() const& {
        if (isError())
            throw std::runtime_error(
                "Attempted to access value of an error Result: " +
                std::get<Error>(data).message);
        return std::get<T>(data);
    }

    /**
     * @brief Gets the success value (rvalue ref).
     * @return An rvalue reference to the success value.
     * @throws std::runtime_error if the result is an error.
     */
    T&& value() && {
        if (isError())
            throw std::runtime_error(
                "Attempted to access value of an error Result: " +
                std::get<Error>(data).message);
        return std::move(std::get<T>(data));
    }

    /**
     * @brief Gets the error message.
     * @return A const reference to the error message string.
     * @throws std::runtime_error if the result is successful.
     */
    const std::string& error() const {
        if (isSuccess())
            throw std::runtime_error(
                "Attempted to access error of a success Result.");
        return std::get<Error>(data).message;
    }
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_RESULT_HPP

#ifndef ATOM_SECRET_RESULT_HPP
#define ATOM_SECRET_RESULT_HPP

#include <stdexcept>
#include <string>
#include <variant>

namespace atom::secret {

/**
 * @brief Template for operation results, alternative to exceptions.
 * @tparam T The type of the successful result value.
 */
template <typename T>
class Result {
private:
    std::variant<T, std::string>
        data;  ///< Holds either the success value or an error string.

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
    explicit Result(const std::string& error) : data(error) {}

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
        return std::holds_alternative<std::string>(data);
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
                std::get<std::string>(data));
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
                std::get<std::string>(data));
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
        return std::get<std::string>(data);
    }
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_RESULT_HPP
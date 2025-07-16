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

// Primary template
template <typename T>
class Result {
private:
    std::variant<T, std::string> data;
    Result(const std::string& error, bool) : data(error) {}
public:
    explicit Result(const T& value) : data(value) {}
    explicit Result(T&& value) noexcept : data(std::move(value)) {}
    static Result<T> Error(const std::string& error) { return Result<T>(error, true); }
    bool isSuccess() const noexcept { return std::holds_alternative<T>(data); }
    bool isError() const noexcept { return std::holds_alternative<std::string>(data); }
    const T& value() const& {
        if (isError())
            throw std::runtime_error("Attempted to access value of an error Result: " + std::get<std::string>(data));
        return std::get<T>(data);
    }
    T&& value() && {
        if (isError())
            throw std::runtime_error("Attempted to access value of an error Result: " + std::get<std::string>(data));
        return std::move(std::get<T>(data));
    }
    const std::string& error() const {
        if (isSuccess())
            throw std::runtime_error("Attempted to access error of a success Result.");
        return std::get<std::string>(data);
    }
};

// Specialization for void
template <>
class Result<void> {
private:
    std::variant<std::monostate, std::string> data;
    Result(const std::string& error, bool) : data(error) {}
public:
    Result() : data(std::monostate{}) {}
    explicit Result(std::monostate) : data(std::monostate{}) {}
    static Result<void> Error(const std::string& error) { return Result<void>(error, true); }
    bool isSuccess() const noexcept { return std::holds_alternative<std::monostate>(data); }
    bool isError() const noexcept { return std::holds_alternative<std::string>(data); }
    void value() const {
        if (isError())
            throw std::runtime_error("Attempted to access value of an error Result: " + std::get<std::string>(data));
    }
    const std::string& error() const {
        if (isSuccess())
            throw std::runtime_error("Attempted to access error of a success Result.");
        return std::get<std::string>(data);
    }
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_RESULT_HPP

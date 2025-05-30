#ifndef ATOM_EXTRA_BOOST_SYSTEM_HPP
#define ATOM_EXTRA_BOOST_SYSTEM_HPP

#if __has_include(<boost/system/error_category.hpp>)
#include <boost/system/error_category.hpp>
#endif
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <optional>
#include <string>
#include <system_error>
#include <type_traits>

namespace atom::extra::boost {

/**
 * @brief A wrapper class for Boost.System error codes
 */
class Error {
public:
    Error() noexcept = default;

    /**
     * @brief Constructs an Error from a Boost.System error code
     * @param error_code The Boost.System error code
     */
    explicit constexpr Error(
        const ::boost::system::error_code& error_code) noexcept
        : m_ec_(error_code) {}

    /**
     * @brief Constructs an Error from an error value and category
     * @param error_value The error value
     * @param error_category The error category
     */
    constexpr Error(
        int error_value,
        const ::boost::system::error_category& error_category) noexcept
        : m_ec_(error_value, error_category) {}

    /**
     * @brief Gets the error value
     * @return The error value
     */
    [[nodiscard]] constexpr int value() const noexcept { return m_ec_.value(); }

    /**
     * @brief Gets the error category
     * @return The error category
     */
    [[nodiscard]] constexpr const ::boost::system::error_category& category()
        const noexcept {
        return m_ec_.category();
    }

    /**
     * @brief Gets the error message
     * @return The error message
     */
    [[nodiscard]] std::string message() const { return m_ec_.message(); }

    /**
     * @brief Checks if the error code is valid
     * @return True if the error code is valid
     */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(m_ec_);
    }

    /**
     * @brief Converts to a Boost.System error code
     * @return The Boost.System error code
     */
    [[nodiscard]] constexpr ::boost::system::error_code toBoostErrorCode()
        const noexcept {
        return m_ec_;
    }

    /**
     * @brief Equality operator
     * @param other The other Error to compare
     * @return True if the errors are equal
     */
    [[nodiscard]] constexpr bool operator==(const Error& other) const noexcept {
        return m_ec_ == other.m_ec_;
    }

    /**
     * @brief Inequality operator
     * @param other The other Error to compare
     * @return True if the errors are not equal
     */
    [[nodiscard]] constexpr bool operator!=(const Error& other) const noexcept {
        return !(*this == other);
    }

private:
    ::boost::system::error_code m_ec_;
};

/**
 * @brief A custom exception class for handling errors
 */
class Exception : public std::system_error {
public:
    /**
     * @brief Constructs an Exception from an Error
     * @param error The Error object
     */
    explicit Exception(const Error& error)
        : std::system_error(error.value(), error.category(), error.message()) {}

    /**
     * @brief Gets the associated Error
     * @return The associated Error
     */
    [[nodiscard]] Error error() const noexcept {
        return Error(::boost::system::error_code(
            code().value(), ::boost::system::generic_category()));
    }
};

/**
 * @brief A class template for handling results with potential errors
 * @tparam T The type of the result value
 */
template <typename T>
class Result {
public:
    using value_type = T;

    /**
     * @brief Constructs a Result with a value
     * @param value The result value
     */
    explicit Result(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_value_(std::move(value)) {}

    /**
     * @brief Constructs a Result with an Error
     * @param error The Error object
     */
    explicit constexpr Result(Error error) noexcept : m_error_(error) {}

    /**
     * @brief Checks if the Result has a value
     * @return True if the Result has a value
     */
    [[nodiscard]] constexpr bool hasValue() const noexcept { return !m_error_; }

    /**
     * @brief Gets the result value
     * @return The result value
     * @throws Exception if there is an error
     */
    [[nodiscard]] const T& value() const& {
        if ((!hasValue())) [[unlikely]] {
            throw Exception(m_error_);
        }
        return *m_value_;
    }

    /**
     * @brief Gets the result value
     * @return The result value
     * @throws Exception if there is an error
     */
    [[nodiscard]] T&& value() && {
        if ((!hasValue())) [[unlikely]] {
            throw Exception(m_error_);
        }
        return std::move(*m_value_);
    }

    /**
     * @brief Gets the associated Error
     * @return The associated Error
     */
    [[nodiscard]] constexpr const Error& error() const& noexcept {
        return m_error_;
    }

    /**
     * @brief Gets the associated Error
     * @return The associated Error
     */
    [[nodiscard]] constexpr Error error() && noexcept { return m_error_; }

    /**
     * @brief Checks if the Result has a value
     * @return True if the Result has a value
     */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return hasValue();
    }

    /**
     * @brief Gets the result value or a default value
     * @tparam U The type of the default value
     * @param default_value The default value
     * @return The result value or the default value
     */
    template <typename U>
    [[nodiscard]] T valueOr(U&& default_value) const& {
        return (hasValue()) ? value()
                            : static_cast<T>(std::forward<U>(default_value));
    }

    /**
     * @brief Applies a function to the result value if it exists
     * @tparam F The type of the function
     * @param func The function to apply
     * @return A new Result with the function applied
     */
    template <typename F>
    [[nodiscard]] auto map(F&& func) const
        -> Result<std::invoke_result_t<F, T>> {
        if ((hasValue())) [[likely]] {
            return Result<std::invoke_result_t<F, T>>(func(*m_value_));
        }
        return Result<std::invoke_result_t<F, T>>(Error(m_error_));
    }

    /**
     * @brief Applies a function to the result value if it exists
     * @tparam F The type of the function
     * @param func The function to apply
     * @return The result of the function
     */
    template <typename F>
    [[nodiscard]] auto andThen(F&& func) const -> std::invoke_result_t<F, T> {
        if ((hasValue())) [[likely]] {
            return func(*m_value_);
        }
        return std::invoke_result_t<F, T>(Error(m_error_));
    }

private:
    std::optional<T> m_value_;
    Error m_error_;
};

/**
 * @brief Specialization of the Result class for void type
 */
template <>
class Result<void> {
public:
    Result() noexcept = default;

    /**
     * @brief Constructs a Result with an Error
     * @param error The Error object
     */
    explicit constexpr Result(Error error) noexcept : m_error_(error) {}

    /**
     * @brief Checks if the Result has a value
     * @return True if the Result has a value
     */
    [[nodiscard]] constexpr bool hasValue() const noexcept { return !m_error_; }

    /**
     * @brief Gets the associated Error
     * @return The associated Error
     */
    [[nodiscard]] constexpr const Error& error() const& noexcept {
        return m_error_;
    }

    /**
     * @brief Gets the associated Error
     * @return The associated Error
     */
    [[nodiscard]] constexpr Error error() && noexcept { return m_error_; }

    /**
     * @brief Checks if the Result has a value
     * @return True if the Result has a value
     */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return hasValue();
    }

private:
    Error m_error_;
};

/**
 * @brief Creates a Result from a function
 * @tparam F The type of the function
 * @param func The function to execute
 * @return A Result with the function's return value or an Error
 */
template <typename F>
[[nodiscard]] auto makeResult(F&& func) -> Result<std::invoke_result_t<F>> {
    using return_type = std::invoke_result_t<F>;
    try {
        if constexpr (std::is_void_v<return_type>) {
            func();
            return Result<void>();
        } else {
            return Result<return_type>(func());
        }
    } catch (const Exception& e) {
        return Result<return_type>(e.error());
    } catch (const std::exception&) {
        return Result<return_type>(
            Error(::boost::system::errc::invalid_argument,
                  ::boost::system::generic_category()));
    }
}

}  // namespace atom::extra::boost

#endif  // ATOM_EXTRA_BOOST_SYSTEM_HPP

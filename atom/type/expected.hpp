#ifndef ATOM_TYPE_EXPECTED_HPP
#define ATOM_TYPE_EXPECTED_HPP

#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace atom::type {

/**
 * @brief A generic error class template that encapsulates error information.
 *
 * The `Error` class is used to represent and store error details. It provides
 * access to the error and supports comparison operations.
 *
 * @tparam E The type of the error.
 */
template <typename E>
class Error {
public:
    /**
     * @brief Constructs an `Error` object with the given error.
     *
     * @param error The error to be stored.
     */
    constexpr explicit Error(E error) noexcept(
        std::is_nothrow_move_constructible_v<E>)
        : error_(std::move(error)) {}

    /**
     * @brief Constructs an `Error` object from a C-style string if the error
     * type is `std::string`.
     *
     * @tparam T The type of the C-style string.
     * @param error The C-style string representing the error.
     */
    template <typename T>
        requires std::is_same_v<E, std::string> &&
                 std::convertible_to<T, std::string>
    constexpr explicit Error(T&& error) : error_(std::forward<T>(error)) {}

    /**
     * @brief Retrieves the stored error.
     *
     * @return A constant reference to the stored error.
     */
    [[nodiscard]] constexpr const E& error() const& noexcept { return error_; }

    /**
     * @brief Retrieves the stored error by move.
     *
     * @return An rvalue reference to the stored error.
     */
    [[nodiscard]] constexpr E&& error() && noexcept {
        return std::move(error_);
    }

    /**
     * @brief Compares two `Error` objects for equality.
     *
     * @param other The other `Error` object to compare with.
     * @return `true` if both errors are equal, `false` otherwise.
     */
    constexpr bool operator==(const Error& other) const
        noexcept(noexcept(error_ == other.error_)) {
        return error_ == other.error_;
    }

private:
    E error_;
};

/**
 * @brief An `unexpected` class template similar to `std::unexpected`.
 *
 * The `unexpected` class is used to represent an error state in the `expected`
 * type.
 *
 * @tparam E The type of the error.
 */
template <typename E>
class unexpected {
public:
    /**
     * @brief Constructs an `unexpected` object with an error.
     *
     * @param error The error to be stored.
     */
    template <typename U = E>
        requires std::constructible_from<E, U>
    constexpr explicit unexpected(U&& error) noexcept(
        std::is_nothrow_constructible_v<E, U>)
        : error_(std::forward<U>(error)) {}

    /**
     * @brief Retrieves the stored error.
     *
     * @return A constant reference to the stored error.
     */
    [[nodiscard]] constexpr const E& error() const& noexcept { return error_; }

    /**
     * @brief Retrieves the stored error by move.
     *
     * @return An rvalue reference to the stored error.
     */
    [[nodiscard]] constexpr E&& error() && noexcept {
        return std::move(error_);
    }

    /**
     * @brief Compares two `unexpected` objects for equality.
     *
     * @param other The other `unexpected` object to compare with.
     * @return `true` if both errors are equal, `false` otherwise.
     */
    constexpr bool operator==(const unexpected& other) const
        noexcept(noexcept(error_ == other.error_)) {
        return error_ == other.error_;
    }

private:
    E error_;
};

template <typename E>
unexpected(E) -> unexpected<E>;

/**
 * @brief The primary `expected` class template.
 *
 * The `expected` class represents a value that may either contain a valid value
 * of type `T` or an error of type `E`. It provides mechanisms to access the
 * value or the error and supports various monadic operations.
 *
 * @tparam T The type of the expected value.
 * @tparam E The type of the error (default is `std::string`).
 */
template <typename T, typename E = std::string>
class expected {
private:
    std::variant<T, Error<E>> value_;

public:
    using value_type = T;
    using error_type = E;

    /**
     * @brief Default constructs an `expected` object containing a
     * default-constructed value.
     *
     * This constructor is only enabled if `T` is default constructible.
     */
    constexpr expected() noexcept(std::is_nothrow_default_constructible_v<T>)
        requires std::is_default_constructible_v<T>
        : value_(std::in_place_index<0>) {}

    /**
     * @brief Constructs an `expected` object containing a value.
     *
     * @param value The value to be stored.
     */
    template <typename U = T>
        requires std::constructible_from<T, U> &&
                 (!std::same_as<std::decay_t<U>, expected>)
    constexpr expected(U&& value) noexcept(
        std::is_nothrow_constructible_v<T, U>)
        : value_(std::in_place_index<0>, std::forward<U>(value)) {}

    /**
     * @brief Constructs an `expected` object containing an error.
     *
     * @param error The error to be stored.
     */
    template <typename U>
        requires std::constructible_from<Error<E>, U>
    constexpr expected(U&& error) noexcept(
        std::is_nothrow_constructible_v<Error<E>, U>)
        : value_(std::forward<U>(error)) {}

    /**
     * @brief Constructs an `expected` object from an `unexpected` error.
     *
     * @param unex The `unexpected` error to be stored.
     */
    template <typename U>
        requires std::constructible_from<E, U>
    constexpr expected(const unexpected<U>& unex) noexcept(
        std::is_nothrow_constructible_v<E, const U&>)
        : value_(Error<E>(unex.error())) {}

    template <typename U>
        requires std::constructible_from<E, U>
    constexpr expected(unexpected<U>&& unex) noexcept(
        std::is_nothrow_constructible_v<E, U>)
        : value_(Error<E>(std::move(unex).error())) {}

    expected(const expected&) = default;
    expected(expected&&) = default;
    expected& operator=(const expected&) = default;
    expected& operator=(expected&&) = default;

    /**
     * @brief Checks if the `expected` object contains a valid value.
     *
     * @return `true` if it contains a value, `false` if it contains an error.
     */
    [[nodiscard]] constexpr bool has_value() const noexcept {
        return value_.index() == 0;
    }

    /**
     * @brief Retrieves a reference to the stored value.
     *
     * @return A reference to the stored value.
     * @throws std::logic_error If the `expected` contains an error.
     */
    [[nodiscard]] constexpr T& value() & {
        if (!has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access value, but it contains an error.");
        }
        return std::get<0>(value_);
    }

    /**
     * @brief Retrieves a constant reference to the stored value.
     *
     * @return A constant reference to the stored value.
     * @throws std::logic_error If the `expected` contains an error.
     */
    [[nodiscard]] constexpr const T& value() const& {
        if (!has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access value, but it contains an error.");
        }
        return std::get<0>(value_);
    }

    /**
     * @brief Retrieves an rvalue reference to the stored value.
     *
     * @return An rvalue reference to the stored value.
     * @throws std::logic_error If the `expected` contains an error.
     */
    [[nodiscard]] constexpr T&& value() && {
        if (!has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access value, but it contains an error.");
        }
        return std::get<0>(std::move(value_));
    }

    /**
     * @brief Retrieves a reference to the stored error.
     *
     * @return A reference to the stored error.
     * @throws std::logic_error If the `expected` contains a value.
     */
    [[nodiscard]] constexpr const Error<E>& error() const& {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(value_);
    }

    [[nodiscard]] constexpr Error<E>& error() & {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(value_);
    }

    [[nodiscard]] constexpr Error<E>&& error() && {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(std::move(value_));
    }

    /**
     * @brief Conversion operator to `bool`.
     *
     * @return `true` if the `expected` contains a value, `false` otherwise.
     */
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /**
     * @brief Applies a function to the stored value if it exists.
     *
     * @tparam Func The type of the function to apply.
     * @param func The function to apply to the stored value.
     * @return The result of the function if a value exists, or an `expected`
     * containing the error.
     */
    template <typename Func>
    constexpr auto and_then(
        Func&& func) & -> decltype(func(std::declval<T&>())) {
        if (has_value()) {
            return func(value());
        }
        return decltype(func(std::declval<T&>()))(error());
    }

    template <typename Func>
    constexpr auto and_then(
        Func&& func) const& -> decltype(func(std::declval<const T&>())) {
        if (has_value()) {
            return func(value());
        }
        return decltype(func(std::declval<const T&>()))(error());
    }

    template <typename Func>
    constexpr auto and_then(
        Func&& func) && -> decltype(func(std::declval<T&&>())) {
        if (has_value()) {
            return func(std::move(value()));
        }
        return decltype(func(std::declval<T&&>()))(std::move(error()));
    }

    /**
     * @brief Applies a function to the stored value if it exists and wraps the
     * result in an `expected`.
     *
     * @tparam Func The type of the function to apply.
     * @param func The function to apply to the stored value.
     * @return An `expected` containing the result of the function, or an
     * `expected` containing the error.
     */
    template <typename Func>
    constexpr auto map(
        Func&& func) & -> expected<decltype(func(std::declval<T&>())), E> {
        using ReturnType = decltype(func(std::declval<T&>()));
        if (has_value()) {
            return expected<ReturnType, E>(func(value()));
        }
        return expected<ReturnType, E>(error());
    }

    template <typename Func>
    constexpr auto map(Func&& func)
        const& -> expected<decltype(func(std::declval<const T&>())), E> {
        using ReturnType = decltype(func(std::declval<const T&>()));
        if (has_value()) {
            return expected<ReturnType, E>(func(value()));
        }
        return expected<ReturnType, E>(error());
    }

    template <typename Func>
    constexpr auto map(
        Func&& func) && -> expected<decltype(func(std::declval<T&&>())), E> {
        using ReturnType = decltype(func(std::declval<T&&>()));
        if (has_value()) {
            return expected<ReturnType, E>(func(std::move(value())));
        }
        return expected<ReturnType, E>(std::move(error()));
    }

    /**
     * @brief Transforms the stored error using the provided function.
     */
    template <typename Func>
    constexpr auto transform_error(
        Func&& func) & -> expected<T, decltype(func(std::declval<E&>()))> {
        using NewErrorType = decltype(func(std::declval<E&>()));
        if (has_value()) {
            return expected<T, NewErrorType>(value());
        }
        return expected<T, NewErrorType>(
            Error<NewErrorType>(func(error().error())));
    }

    template <typename Func>
    constexpr auto transform_error(Func&& func)
        const& -> expected<T, decltype(func(std::declval<const E&>()))> {
        using NewErrorType = decltype(func(std::declval<const E&>()));
        if (has_value()) {
            return expected<T, NewErrorType>(value());
        }
        return expected<T, NewErrorType>(
            Error<NewErrorType>(func(error().error())));
    }

    template <typename Func>
    constexpr auto transform_error(
        Func&& func) && -> expected<T, decltype(func(std::declval<E&&>()))> {
        using NewErrorType = decltype(func(std::declval<E&&>()));
        if (has_value()) {
            return expected<T, NewErrorType>(std::move(value()));
        }
        return expected<T, NewErrorType>(
            Error<NewErrorType>(func(std::move(error().error()))));
    }

    /**
     * @brief Compares two `expected` objects for equality.
     */
    constexpr bool operator==(const expected& other) const
        noexcept(noexcept(std::declval<T>() == std::declval<T>()) &&
                 noexcept(std::declval<Error<E>>() ==
                          std::declval<Error<E>>())) {
        if (has_value() != other.has_value()) {
            return false;
        }
        if (has_value()) {
            return std::get<0>(value_) == std::get<0>(other.value_);
        }
        return std::get<1>(value_) == std::get<1>(other.value_);
    }
};

/**
 * @brief Specialization of the `expected` class template for `void` type.
 *
 * This specialization handles cases where no value is expected, only an error.
 *
 * @tparam E The type of the error.
 */
template <typename E>
class expected<void, E> {
private:
    std::variant<std::monostate, Error<E>> value_;

public:
    using value_type = void;
    using error_type = E;

    /**
     * @brief Default constructs an `expected` object containing no value.
     */
    constexpr expected() noexcept : value_(std::monostate{}) {}

    /**
     * @brief Constructs an `expected` object containing an error.
     *
     * @param error The error to be stored.
     */
    template <typename U>
        requires std::constructible_from<Error<E>, U>
    constexpr expected(U&& error) noexcept(
        std::is_nothrow_constructible_v<Error<E>, U>)
        : value_(std::forward<U>(error)) {}

    /**
     * @brief Constructs an `expected` object from an `unexpected` error.
     *
     * @param unex The `unexpected` error to be stored.
     */
    template <typename U>
        requires std::constructible_from<E, U>
    constexpr expected(const unexpected<U>& unex) noexcept(
        std::is_nothrow_constructible_v<E, const U&>)
        : value_(Error<E>(unex.error())) {}

    template <typename U>
        requires std::constructible_from<E, U>
    constexpr expected(unexpected<U>&& unex) noexcept(
        std::is_nothrow_constructible_v<E, U>)
        : value_(Error<E>(std::move(unex).error())) {}

    /**
     * @brief Checks if the `expected` object contains a valid value.
     *
     * @return `true` if it contains a value, `false` if it contains an error.
     */
    [[nodiscard]] constexpr bool has_value() const noexcept {
        return value_.index() == 0;
    }

    /**
     * @brief Retrieves the stored value.
     *
     * Since the value type is `void`, this function does nothing but can throw
     * if an error exists.
     *
     * @throws std::logic_error If the `expected` contains an error.
     */
    constexpr void value() const {
        if (!has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access value, but it contains an error.");
        }
    }

    /**
     * @brief Retrieves a reference to the stored error.
     *
     * @return A reference to the stored error.
     * @throws std::logic_error If the `expected` contains a value.
     */
    [[nodiscard]] constexpr const Error<E>& error() const& {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(value_);
    }

    [[nodiscard]] constexpr Error<E>& error() & {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(value_);
    }

    [[nodiscard]] constexpr Error<E>&& error() && {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(std::move(value_));
    }

    /**
     * @brief Conversion operator to `bool`.
     *
     * @return `true` if the `expected` contains a value, `false` otherwise.
     */
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /**
     * @brief Applies a function to the `expected` object if it contains a
     * value.
     *
     * @tparam Func The type of the function to apply.
     * @param func The function to apply.
     * @return The result of the function if a value exists, or an `expected`
     * containing the error.
     */
    template <typename Func>
    constexpr auto and_then(Func&& func) & -> decltype(func()) {
        if (has_value()) {
            return func();
        }
        return decltype(func())(error());
    }

    template <typename Func>
    constexpr auto and_then(Func&& func) const& -> decltype(func()) {
        if (has_value()) {
            return func();
        }
        return decltype(func())(error());
    }

    template <typename Func>
    constexpr auto and_then(Func&& func) && -> decltype(func()) {
        if (has_value()) {
            return func();
        }
        return decltype(func())(std::move(error()));
    }

    /**
     * @brief Transforms the stored error using the provided function.
     *
     * @tparam Func The type of the function to apply to the error.
     * @param func The function to apply to the stored error.
     * @return An `expected` with the transformed error type if an error exists,
     * otherwise the original `expected`.
     */
    template <typename Func>
    constexpr auto transform_error(
        Func&& func) & -> expected<void, decltype(func(std::declval<E&>()))> {
        using NewErrorType = decltype(func(std::declval<E&>()));
        if (has_value()) {
            return expected<void, NewErrorType>{};
        }
        return expected<void, NewErrorType>(
            Error<NewErrorType>(func(error().error())));
    }

    template <typename Func>
    constexpr auto transform_error(Func&& func)
        const& -> expected<void, decltype(func(std::declval<const E&>()))> {
        using NewErrorType = decltype(func(std::declval<const E&>()));
        if (has_value()) {
            return expected<void, NewErrorType>{};
        }
        return expected<void, NewErrorType>(
            Error<NewErrorType>(func(error().error())));
    }

    template <typename Func>
    constexpr auto transform_error(
        Func&& func) && -> expected<void, decltype(func(std::declval<E&&>()))> {
        using NewErrorType = decltype(func(std::declval<E&&>()));
        if (has_value()) {
            return expected<void, NewErrorType>{};
        }
        return expected<void, NewErrorType>(
            Error<NewErrorType>(func(std::move(error().error()))));
    }

    /**
     * @brief Compares two `expected<void, E>` objects for equality.
     */
    constexpr bool operator==(const expected& other) const
        noexcept(noexcept(std::declval<Error<E>>() ==
                          std::declval<Error<E>>())) {
        if (has_value() != other.has_value()) {
            return false;
        }
        if (has_value()) {
            return true;
        }
        return std::get<1>(value_) == std::get<1>(other.value_);
    }
};

/**
 * @brief Creates an `expected` object containing the given value.
 *
 * @tparam T The type of the value.
 * @param value The value to be stored in the `expected`.
 * @return An `expected` object containing the value.
 */
template <typename T>
constexpr auto make_expected(T&& value) -> expected<std::decay_t<T>> {
    return expected<std::decay_t<T>>(std::forward<T>(value));
}

/**
 * @brief Creates an `unexpected` object containing the given error.
 *
 * @tparam E The type of the error.
 * @param error The error to be stored in the `unexpected`.
 * @return An `unexpected` object containing the error.
 */
template <typename E>
constexpr auto make_unexpected(E&& error) -> unexpected<std::decay_t<E>> {
    return unexpected<std::decay_t<E>>(std::forward<E>(error));
}

/**
 * @brief Creates an `unexpected` object containing a `std::string` error from a
 * C-style string.
 *
 * @param error The C-style string representing the error.
 * @return An `unexpected<std::string>` object containing the error.
 */
constexpr auto make_unexpected(const char* error) -> unexpected<std::string> {
    return unexpected<std::string>(std::string(error));
}

}  // namespace atom::type

#endif  // ATOM_TYPE_EXPECTED_HPP

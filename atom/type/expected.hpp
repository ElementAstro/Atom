#ifndef ATOM_TYPE_EXPECTED_HPP
#define ATOM_TYPE_EXPECTED_HPP

#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace atom::type {

/**
 * @brief A generic error class template that encapsulates error information.
 *
 * This class provides a type-safe wrapper around error values, allowing for
 * better error handling and propagation in functional programming patterns.
 *
 * @tparam E The type of the error value
 */
template <typename E>
class Error {
public:
    /**
     * @brief Constructs an Error with the given error value.
     *
     * @param error The error value to wrap
     */
    constexpr explicit Error(E error) noexcept(
        std::is_nothrow_move_constructible_v<E>)
        : error_(std::move(error)) {}

    /**
     * @brief Constructs an Error from a convertible type (specialized for
     * std::string).
     *
     * @tparam T The type to convert from
     * @param error The error value to convert and wrap
     */
    template <typename T>
        requires std::is_same_v<E, std::string> &&
                 std::convertible_to<T, std::string>
    constexpr explicit Error(T&& error) : error_(std::forward<T>(error)) {}

    /**
     * @brief Gets a const reference to the wrapped error value.
     *
     * @return const E& The error value
     */
    [[nodiscard]] constexpr const E& error() const& noexcept { return error_; }

    /**
     * @brief Gets an rvalue reference to the wrapped error value.
     *
     * @return E&& The error value as an rvalue reference
     */
    [[nodiscard]] constexpr E&& error() && noexcept {
        return std::move(error_);
    }

    /**
     * @brief Compares two Error objects for equality.
     *
     * @param other The other Error object to compare with
     * @return true if the error values are equal, false otherwise
     */
    constexpr bool operator==(const Error& other) const
        noexcept(noexcept(error_ == other.error_)) {
        return error_ == other.error_;
    }

    /**
     * @brief Compares two Error objects for inequality.
     *
     * @param other The other Error object to compare with
     * @return true if the error values are not equal, false otherwise
     */
    constexpr bool operator!=(const Error& other) const
        noexcept(noexcept(error_ != other.error_)) {
        return error_ != other.error_;
    }

private:
    E error_;
};

/**
 * @brief An `unexpected` class template similar to `std::unexpected`.
 *
 * This class represents an unexpected error value that can be used to construct
 * an expected object in an error state.
 *
 * @tparam E The type of the error value
 */
template <typename E>
class unexpected {
public:
    /**
     * @brief Constructs an unexpected error value.
     *
     * @tparam U The type of the error value (defaults to E)
     * @param error The error value
     */
    template <typename U = E>
        requires std::constructible_from<E, U>
    constexpr explicit unexpected(U&& error) noexcept(
        std::is_nothrow_constructible_v<E, U>)
        : error_(std::forward<U>(error)) {}

    /**
     * @brief Gets a const reference to the error value.
     *
     * @return const E& The error value
     */
    [[nodiscard]] constexpr const E& error() const& noexcept { return error_; }

    /**
     * @brief Gets an rvalue reference to the error value.
     *
     * @return E&& The error value as an rvalue reference
     */
    [[nodiscard]] constexpr E&& error() && noexcept {
        return std::move(error_);
    }

    /**
     * @brief Compares two unexpected objects for equality.
     *
     * @param other The other unexpected object to compare with
     * @return true if the error values are equal, false otherwise
     */
    constexpr bool operator==(const unexpected& other) const
        noexcept(noexcept(error_ == other.error_)) {
        return error_ == other.error_;
    }

    /**
     * @brief Compares two unexpected objects for inequality.
     *
     * @param other The other unexpected object to compare with
     * @return true if the error values are not equal, false otherwise
     */
    constexpr bool operator!=(const unexpected& other) const
        noexcept(noexcept(error_ != other.error_)) {
        return error_ != other.error_;
    }

private:
    E error_;
};

/// Deduction guide for unexpected
template <typename E>
unexpected(E) -> unexpected<E>;

/**
 * @brief A class template representing a value that may be either a valid value
 * or an error.
 *
 * This is similar to std::expected (C++23) but provides additional
 * functionality and monadic operations for better error handling in a
 * functional programming style.
 *
 * @tparam T The type of the expected value
 * @tparam E The type of the error (defaults to std::string)
 */
template <typename T, typename E = std::string>
class expected {
private:
    std::variant<T, Error<E>> value_;

public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;

    /**
     * @brief Default constructor - constructs an expected with a
     * default-constructed value.
     */
    constexpr expected() noexcept(std::is_nothrow_default_constructible_v<T>)
        requires std::is_default_constructible_v<T>
        : value_(std::in_place_index<0>) {}

    /**
     * @brief Constructs an expected with a value.
     *
     * @tparam U The type of the value to construct from
     * @param value The value to store
     */
    template <typename U = T>
        requires std::constructible_from<T, U> &&
                 (!std::same_as<std::decay_t<U>, expected>) &&
                 (!std::same_as<std::decay_t<U>, unexpected<E>>) &&
                 (!std::same_as<std::decay_t<U>, Error<E>>) &&
                 (!std::same_as<std::decay_t<U>, std::in_place_t>)
    constexpr expected(U&& value) noexcept(
        std::is_nothrow_constructible_v<T, U>)
        : value_(std::in_place_index<0>, std::forward<U>(value)) {}

    /**
     * @brief Constructs an expected in-place with the given arguments.
     *
     * @tparam Args The types of the arguments
     * @param in_place Tag to indicate in-place construction
     * @param args The arguments to forward to T's constructor
     */
    template <typename... Args>
        requires std::constructible_from<T, Args...>
    constexpr explicit expected(std::in_place_t, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args...>)
        : value_(std::in_place_index<0>, std::forward<Args>(args)...) {}

    /**
     * @brief Constructs an expected with an error from an Error object.
     *
     * @tparam U The type of the error
     * @param error The error to store
     */
    template <typename U>
        requires std::constructible_from<Error<E>, U> &&
                 (!std::constructible_from<T, U>)
    constexpr expected(U&& error) noexcept(
        std::is_nothrow_constructible_v<Error<E>, U>)
        : value_(std::forward<U>(error)) {}

    /**
     * @brief Constructs an expected with an error from an unexpected object.
     *
     * @tparam U The type of the unexpected error
     * @param unex The unexpected error
     */
    template <typename U>
        requires std::constructible_from<E, U>
    constexpr expected(const unexpected<U>& unex) noexcept(
        std::is_nothrow_constructible_v<E, const U&>)
        : value_(Error<E>(unex.error())) {}

    /**
     * @brief Constructs an expected with an error from an unexpected object
     * (move version).
     *
     * @tparam U The type of the unexpected error
     * @param unex The unexpected error
     */
    template <typename U>
        requires std::constructible_from<E, U>
    constexpr expected(unexpected<U>&& unex) noexcept(
        std::is_nothrow_constructible_v<E, U>)
        : value_(Error<E>(std::move(unex).error())) {}

    // Copy and move constructors
    expected(const expected&) = default;
    expected(expected&&) = default;
    expected& operator=(const expected&) = default;
    expected& operator=(expected&&) = default;

    /**
     * @brief Assigns a value to the expected.
     *
     * @tparam U The type of the value
     * @param value The value to assign
     * @return expected& Reference to this object
     */
    template <typename U>
        requires std::assignable_from<T&, U> &&
                 (!std::same_as<std::decay_t<U>, expected>) &&
                 (!std::same_as<std::decay_t<U>, unexpected<E>>)
    constexpr expected& operator=(U&& value) {
        if (has_value()) {
            std::get<0>(value_) = std::forward<U>(value);
        } else {
            value_.template emplace<0>(std::forward<U>(value));
        }
        return *this;
    }

    /**
     * @brief Assigns an error from an unexpected object.
     *
     * @tparam U The type of the unexpected error
     * @param unex The unexpected error
     * @return expected& Reference to this object
     */
    template <typename U>
        requires std::constructible_from<E, U>
    constexpr expected& operator=(const unexpected<U>& unex) {
        if (has_value()) {
            value_.template emplace<1>(Error<E>(unex.error()));
        } else {
            std::get<1>(value_) = Error<E>(unex.error());
        }
        return *this;
    }

    /**
     * @brief Assigns an error from an unexpected object (move version).
     *
     * @tparam U The type of the unexpected error
     * @param unex The unexpected error
     * @return expected& Reference to this object
     */
    template <typename U>
        requires std::constructible_from<E, U>
    constexpr expected& operator=(unexpected<U>&& unex) {
        if (has_value()) {
            value_.template emplace<1>(Error<E>(std::move(unex).error()));
        } else {
            std::get<1>(value_) = Error<E>(std::move(unex).error());
        }
        return *this;
    }

    /**
     * @brief Checks if the expected contains a value (not an error).
     *
     * @return true if contains a value, false if contains an error
     */
    [[nodiscard]] constexpr bool has_value() const noexcept {
        return value_.index() == 0;
    }

    /**
     * @brief Gets the stored value with bounds checking.
     *
     * @return T& Reference to the stored value
     * @throws std::logic_error if the expected contains an error
     */
    [[nodiscard]] constexpr T& value() & {
        if (!has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access value, but it contains an error.");
        }
        return std::get<0>(value_);
    }

    /**
     * @brief Gets the stored value with bounds checking (const version).
     *
     * @return const T& Const reference to the stored value
     * @throws std::logic_error if the expected contains an error
     */
    [[nodiscard]] constexpr const T& value() const& {
        if (!has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access value, but it contains an error.");
        }
        return std::get<0>(value_);
    }

    /**
     * @brief Gets the stored value with bounds checking (move version).
     *
     * @return T&& Rvalue reference to the stored value
     * @throws std::logic_error if the expected contains an error
     */
    [[nodiscard]] constexpr T&& value() && {
        if (!has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access value, but it contains an error.");
        }
        return std::get<0>(std::move(value_));
    }

    /**
     * @brief Gets the stored value or a default value if an error is present.
     *
     * @tparam U The type of the default value
     * @param default_value The default value to return if an error is present
     * @return T The stored value or the default value
     */
    template <typename U>
    [[nodiscard]] constexpr T value_or(U&& default_value) const& {
        return has_value() ? std::get<0>(value_)
                           : static_cast<T>(std::forward<U>(default_value));
    }

    /**
     * @brief Gets the stored value or a default value if an error is present
     * (move version).
     *
     * @tparam U The type of the default value
     * @param default_value The default value to return if an error is present
     * @return T The stored value or the default value
     */
    template <typename U>
    [[nodiscard]] constexpr T value_or(U&& default_value) && {
        return has_value() ? std::get<0>(std::move(value_))
                           : static_cast<T>(std::forward<U>(default_value));
    }

    /**
     * @brief Gets the stored value or invokes a function to get a default
     * value.
     *
     * @tparam F The type of the function
     * @param func Function to invoke if an error is present
     * @return T The stored value or the result of invoking func
     */
    template <typename F>
        requires std::invocable<F> &&
                 std::convertible_to<std::invoke_result_t<F>, T>
    [[nodiscard]] constexpr T value_or_else(F&& func) const& {
        return has_value() ? std::get<0>(value_)
                           : static_cast<T>(std::invoke(std::forward<F>(func)));
    }

    /**
     * @brief Gets the stored value or invokes a function to get a default value
     * (move version).
     *
     * @tparam F The type of the function
     * @param func Function to invoke if an error is present
     * @return T The stored value or the result of invoking func
     */
    template <typename F>
        requires std::invocable<F> &&
                 std::convertible_to<std::invoke_result_t<F>, T>
    [[nodiscard]] constexpr T value_or_else(F&& func) && {
        return has_value() ? std::get<0>(std::move(value_))
                           : static_cast<T>(std::invoke(std::forward<F>(func)));
    }

    /**
     * @brief Dereference operator for convenient value access.
     *
     * @return T& Reference to the stored value
     * @note No bounds checking is performed. Use only when you know the
     * expected contains a value.
     */
    [[nodiscard]] constexpr T& operator*() & noexcept {
        return std::get<0>(value_);
    }

    /**
     * @brief Dereference operator for convenient value access (const version).
     *
     * @return const T& Const reference to the stored value
     * @note No bounds checking is performed. Use only when you know the
     * expected contains a value.
     */
    [[nodiscard]] constexpr const T& operator*() const& noexcept {
        return std::get<0>(value_);
    }

    /**
     * @brief Dereference operator for convenient value access (move version).
     *
     * @return T&& Rvalue reference to the stored value
     * @note No bounds checking is performed. Use only when you know the
     * expected contains a value.
     */
    [[nodiscard]] constexpr T&& operator*() && noexcept {
        return std::get<0>(std::move(value_));
    }

    /**
     * @brief Arrow operator for convenient member access.
     *
     * @return T* Pointer to the stored value
     * @note No bounds checking is performed. Use only when you know the
     * expected contains a value.
     */
    [[nodiscard]] constexpr T* operator->() noexcept {
        return &std::get<0>(value_);
    }

    /**
     * @brief Arrow operator for convenient member access (const version).
     *
     * @return const T* Const pointer to the stored value
     * @note No bounds checking is performed. Use only when you know the
     * expected contains a value.
     */
    [[nodiscard]] constexpr const T* operator->() const noexcept {
        return &std::get<0>(value_);
    }

    /**
     * @brief Gets the stored error with bounds checking.
     *
     * @return const Error<E>& Const reference to the stored error
     * @throws std::logic_error if the expected contains a value
     */
    [[nodiscard]] constexpr const Error<E>& error() const& {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(value_);
    }

    /**
     * @brief Gets the stored error with bounds checking.
     *
     * @return Error<E>& Reference to the stored error
     * @throws std::logic_error if the expected contains a value
     */
    [[nodiscard]] constexpr Error<E>& error() & {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(value_);
    }

    /**
     * @brief Gets the stored error with bounds checking (move version).
     *
     * @return Error<E>&& Rvalue reference to the stored error
     * @throws std::logic_error if the expected contains a value
     */
    [[nodiscard]] constexpr Error<E>&& error() && {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(std::move(value_));
    }

    /**
     * @brief Conversion to bool operator.
     *
     * @return true if the expected contains a value, false if it contains an
     * error
     */
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /**
     * @brief Constructs a new expected in-place.
     *
     * @tparam Args The types of the arguments
     * @param args The arguments to forward to T's constructor
     * @return T& Reference to the newly constructed value
     */
    template <typename... Args>
        requires std::constructible_from<T, Args...>
    constexpr T& emplace(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args...>) {
        return value_.template emplace<0>(std::forward<Args>(args)...);
    }

    /**
     * @brief Swaps the contents of two expected objects.
     *
     * @param other The other expected object to swap with
     */
    constexpr void swap(expected& other) noexcept(
        std::is_nothrow_swappable_v<T> &&
        std::is_nothrow_swappable_v<Error<E>>) {
        value_.swap(other.value_);
    }

    // Monadic operations

    /**
     * @brief Monadic bind operation - chains computations that may fail.
     *
     * @tparam Func The type of the function
     * @param func Function to apply to the value if present
     * @return The result of func if has_value(), otherwise propagates the error
     */
    template <typename Func>
    constexpr auto and_then(
        Func&& func) & -> decltype(func(std::declval<T&>())) {
        if (has_value()) {
            return func(value());
        }
        return decltype(func(std::declval<T&>()))(error());
    }

    /**
     * @brief Monadic bind operation - chains computations that may fail (const
     * version).
     *
     * @tparam Func The type of the function
     * @param func Function to apply to the value if present
     * @return The result of func if has_value(), otherwise propagates the error
     */
    template <typename Func>
    constexpr auto and_then(
        Func&& func) const& -> decltype(func(std::declval<const T&>())) {
        if (has_value()) {
            return func(value());
        }
        return decltype(func(std::declval<const T&>()))(error());
    }

    /**
     * @brief Monadic bind operation - chains computations that may fail (move
     * version).
     *
     * @tparam Func The type of the function
     * @param func Function to apply to the value if present
     * @return The result of func if has_value(), otherwise propagates the error
     */
    template <typename Func>
    constexpr auto and_then(
        Func&& func) && -> decltype(func(std::declval<T&&>())) {
        if (has_value()) {
            return func(std::move(value()));
        }
        return decltype(func(std::declval<T&&>()))(std::move(error()));
    }

    /**
     * @brief Transforms the value if present, wrapping the result in a new
     * expected.
     *
     * @tparam Func The type of the transformation function
     * @param func Function to apply to the value
     * @return A new expected with the transformed value or the original error
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

    /**
     * @brief Transforms the value if present, wrapping the result in a new
     * expected (const version).
     *
     * @tparam Func The type of the transformation function
     * @param func Function to apply to the value
     * @return A new expected with the transformed value or the original error
     */
    template <typename Func>
    constexpr auto map(Func&& func)
        const& -> expected<decltype(func(std::declval<const T&>())), E> {
        using ReturnType = decltype(func(std::declval<const T&>()));
        if (has_value()) {
            return expected<ReturnType, E>(func(value()));
        }
        return expected<ReturnType, E>(error());
    }

    /**
     * @brief Transforms the value if present, wrapping the result in a new
     * expected (move version).
     *
     * @tparam Func The type of the transformation function
     * @param func Function to apply to the value
     * @return A new expected with the transformed value or the original error
     */
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
     * @brief Transforms the error if present, keeping the value unchanged.
     *
     * @tparam Func The type of the transformation function
     * @param func Function to apply to the error
     * @return A new expected with the same value or transformed error
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

    /**
     * @brief Transforms the error if present, keeping the value unchanged
     * (const version).
     *
     * @tparam Func The type of the transformation function
     * @param func Function to apply to the error
     * @return A new expected with the same value or transformed error
     */
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

    /**
     * @brief Transforms the error if present, keeping the value unchanged (move
     * version).
     *
     * @tparam Func The type of the transformation function
     * @param func Function to apply to the error
     * @return A new expected with the same value or transformed error
     */
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
     * @brief Applies a function to the error if present, returning the original
     * expected if it has a value.
     *
     * @tparam Func The type of the function
     * @param func Function to apply to the error
     * @return The result of func if has error, otherwise returns this expected
     */
    template <typename Func>
    constexpr auto or_else(
        Func&& func) & -> decltype(func(std::declval<E&>())) {
        if (has_value()) {
            return *this;
        }
        return func(error().error());
    }

    /**
     * @brief Applies a function to the error if present, returning the original
     * expected if it has a value (const version).
     *
     * @tparam Func The type of the function
     * @param func Function to apply to the error
     * @return The result of func if has error, otherwise returns this expected
     */
    template <typename Func>
    constexpr auto or_else(
        Func&& func) const& -> decltype(func(std::declval<const E&>())) {
        if (has_value()) {
            return *this;
        }
        return func(error().error());
    }

    /**
     * @brief Applies a function to the error if present, returning the original
     * expected if it has a value (move version).
     *
     * @tparam Func The type of the function
     * @param func Function to apply to the error
     * @return The result of func if has error, otherwise returns this expected
     */
    template <typename Func>
    constexpr auto or_else(
        Func&& func) && -> decltype(func(std::declval<E&&>())) {
        if (has_value()) {
            return std::move(*this);
        }
        return func(std::move(error().error()));
    }

    /**
     * @brief Compares two expected objects for equality.
     *
     * @param other The other expected object to compare with
     * @return true if both contain equal values or equal errors, false
     * otherwise
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

    /**
     * @brief Compares two expected objects for inequality.
     *
     * @param other The other expected object to compare with
     * @return true if the objects are not equal, false otherwise
     */
    constexpr bool operator!=(const expected& other) const
        noexcept(noexcept(!(*this == other))) {
        return !(*this == other);
    }
};

/**
 * @brief Specialization of expected for void type.
 *
 * This specialization is used when the expected value type is void,
 * meaning the expected either represents success (with no value) or an error.
 *
 * @tparam E The type of the error
 */
template <typename E>
class expected<void, E> {
private:
    std::variant<std::monostate, Error<E>> value_;

public:
    using value_type = void;
    using error_type = E;
    using unexpected_type = unexpected<E>;

    /**
     * @brief Default constructor - constructs an expected in a success state.
     */
    constexpr expected() noexcept : value_(std::monostate{}) {}

    /**
     * @brief Constructs an expected with an error.
     *
     * @tparam U The type of the error
     * @param error The error to store
     */
    template <typename U>
        requires std::constructible_from<Error<E>, U>
    constexpr expected(U&& error) noexcept(
        std::is_nothrow_constructible_v<Error<E>, U>)
        : value_(std::forward<U>(error)) {}

    /**
     * @brief Constructs an expected with an error from an unexpected object.
     *
     * @tparam U The type of the unexpected error
     * @param unex The unexpected error
     */
    template <typename U>
        requires std::constructible_from<E, U>
    constexpr expected(const unexpected<U>& unex) noexcept(
        std::is_nothrow_constructible_v<E, const U&>)
        : value_(Error<E>(unex.error())) {}

    /**
     * @brief Constructs an expected with an error from an unexpected object
     * (move version).
     *
     * @tparam U The type of the unexpected error
     * @param unex The unexpected error
     */
    template <typename U>
        requires std::constructible_from<E, U>
    constexpr expected(unexpected<U>&& unex) noexcept(
        std::is_nothrow_constructible_v<E, U>)
        : value_(Error<E>(std::move(unex).error())) {}

    /**
     * @brief Checks if the expected represents success (no error).
     *
     * @return true if successful, false if contains an error
     */
    [[nodiscard]] constexpr bool has_value() const noexcept {
        return value_.index() == 0;
    }

    /**
     * @brief Validates that the expected contains a success state.
     *
     * @throws std::logic_error if the expected contains an error
     */
    constexpr void value() const {
        if (!has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access value, but it contains an error.");
        }
    }

    /**
     * @brief Gets the stored error with bounds checking.
     *
     * @return const Error<E>& Const reference to the stored error
     * @throws std::logic_error if the expected represents success
     */
    [[nodiscard]] constexpr const Error<E>& error() const& {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(value_);
    }

    /**
     * @brief Gets the stored error with bounds checking.
     *
     * @return Error<E>& Reference to the stored error
     * @throws std::logic_error if the expected represents success
     */
    [[nodiscard]] constexpr Error<E>& error() & {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(value_);
    }

    /**
     * @brief Gets the stored error with bounds checking (move version).
     *
     * @return Error<E>&& Rvalue reference to the stored error
     * @throws std::logic_error if the expected represents success
     */
    [[nodiscard]] constexpr Error<E>&& error() && {
        if (has_value()) [[unlikely]] {
            throw std::logic_error(
                "Attempted to access error, but it contains a value.");
        }
        return std::get<1>(std::move(value_));
    }

    /**
     * @brief Conversion to bool operator.
     *
     * @return true if the expected represents success, false if it contains an
     * error
     */
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /**
     * @brief Swaps the contents of two expected objects.
     *
     * @param other The other expected object to swap with
     */
    constexpr void swap(expected& other) noexcept(
        std::is_nothrow_swappable_v<Error<E>>) {
        value_.swap(other.value_);
    }

    /**
     * @brief Monadic bind operation for void expected.
     *
     * @tparam Func The type of the function
     * @param func Function to apply if successful
     * @return The result of func if has_value(), otherwise propagates the error
     */
    template <typename Func>
    constexpr auto and_then(Func&& func) & -> decltype(func()) {
        if (has_value()) {
            return func();
        }
        return decltype(func())(error());
    }

    /**
     * @brief Monadic bind operation for void expected (const version).
     *
     * @tparam Func The type of the function
     * @param func Function to apply if successful
     * @return The result of func if has_value(), otherwise propagates the error
     */
    template <typename Func>
    constexpr auto and_then(Func&& func) const& -> decltype(func()) {
        if (has_value()) {
            return func();
        }
        return decltype(func())(error());
    }

    /**
     * @brief Monadic bind operation for void expected (move version).
     *
     * @tparam Func The type of the function
     * @param func Function to apply if successful
     * @return The result of func if has_value(), otherwise propagates the error
     */
    template <typename Func>
    constexpr auto and_then(Func&& func) && -> decltype(func()) {
        if (has_value()) {
            return func();
        }
        return decltype(func())(std::move(error()));
    }

    /**
     * @brief Transforms the error if present.
     *
     * @tparam Func The type of the transformation function
     * @param func Function to apply to the error
     * @return A new expected with success or transformed error
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

    /**
     * @brief Transforms the error if present (const version).
     *
     * @tparam Func The type of the transformation function
     * @param func Function to apply to the error
     * @return A new expected with success or transformed error
     */
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

    /**
     * @brief Transforms the error if present (move version).
     *
     * @tparam Func The type of the transformation function
     * @param func Function to apply to the error
     * @return A new expected with success or transformed error
     */
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
     * @brief Applies a function to the error if present, returning success
     * state if no error.
     *
     * @tparam Func The type of the function
     * @param func Function to apply to the error
     * @return The result of func if has error, otherwise returns this expected
     */
    template <typename Func>
    constexpr auto or_else(
        Func&& func) & -> decltype(func(std::declval<E&>())) {
        if (has_value()) {
            return *this;
        }
        return func(error().error());
    }

    /**
     * @brief Applies a function to the error if present, returning success
     * state if no error (const version).
     *
     * @tparam Func The type of the function
     * @param func Function to apply to the error
     * @return The result of func if has error, otherwise returns this expected
     */
    template <typename Func>
    constexpr auto or_else(
        Func&& func) const& -> decltype(func(std::declval<const E&>())) {
        if (has_value()) {
            return *this;
        }
        return func(error().error());
    }

    /**
     * @brief Applies a function to the error if present, returning success
     * state if no error (move version).
     *
     * @tparam Func The type of the function
     * @param func Function to apply to the error
     * @return The result of func if has error, otherwise returns this expected
     */
    template <typename Func>
    constexpr auto or_else(
        Func&& func) && -> decltype(func(std::declval<E&&>())) {
        if (has_value()) {
            return std::move(*this);
        }
        return func(std::move(error().error()));
    }

    /**
     * @brief Compares two void expected objects for equality.
     *
     * @param other The other expected object to compare with
     * @return true if both represent success or both contain equal errors,
     * false otherwise
     */
    constexpr bool operator==(const expected& other) const
        noexcept(noexcept(std::declval<Error<E>>() ==
                          std::declval<Error<E>>())) {
        if (has_value() != other.has_value()) {
            return false;
        }
        if (has_value()) {
            return true;  // Both successful
        }
        return std::get<1>(value_) == std::get<1>(other.value_);
    }

    /**
     * @brief Compares two void expected objects for inequality.
     *
     * @param other The other expected object to compare with
     * @return true if the objects are not equal, false otherwise
     */
    constexpr bool operator!=(const expected& other) const
        noexcept(noexcept(!(*this == other))) {
        return !(*this == other);
    }
};

// Deduction guides
template <typename T>
expected(T) -> expected<std::decay_t<T>>;

template <typename E>
expected(unexpected<E>) -> expected<void, E>;

/**
 * @brief Creates an expected object with a value.
 *
 * @tparam T The type of the value
 * @param value The value to wrap
 * @return expected<std::decay_t<T>> An expected containing the value
 */
template <typename T>
constexpr auto make_expected(T&& value) -> expected<std::decay_t<T>> {
    return expected<std::decay_t<T>>(std::forward<T>(value));
}

/**
 * @brief Creates an unexpected error object.
 *
 * @tparam E The type of the error
 * @param error The error value
 * @return unexpected<std::decay_t<E>> An unexpected containing the error
 */
template <typename E>
constexpr auto make_unexpected(E&& error) -> unexpected<std::decay_t<E>> {
    return unexpected<std::decay_t<E>>(std::forward<E>(error));
}

/**
 * @brief Creates an unexpected error object from a C-style string.
 *
 * @param error The error string
 * @return unexpected<std::string> An unexpected containing the error string
 */
constexpr auto make_unexpected(const char* error) -> unexpected<std::string> {
    return unexpected<std::string>(std::string(error));
}

/**
 * @brief Creates an unexpected error object from a string literal.
 *
 * @tparam N The size of the string literal
 * @param error The error string literal
 * @return unexpected<std::string> An unexpected containing the error string
 */
template <std::size_t N>
constexpr auto make_unexpected(const char (&error)[N])
    -> unexpected<std::string> {
    return unexpected<std::string>(std::string(error));
}

/**
 * @brief Swaps two expected objects.
 *
 * @tparam T The value type
 * @tparam E The error type
 * @param lhs The first expected object
 * @param rhs The second expected object
 */
template <typename T, typename E>
constexpr void swap(expected<T, E>& lhs,
                    expected<T, E>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

}  // namespace atom::type

#endif  // ATOM_TYPE_EXPECTED_HPP

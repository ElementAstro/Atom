/*
 * optional.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-10

Description: A robust implementation of optional. Using modern C++ features.

**************************************************/

#ifndef ATOM_TYPE_OPTIONAL_HPP
#define ATOM_TYPE_OPTIONAL_HPP

#include <compare>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace atom::type {

class OptionalAccessError : public std::runtime_error {
public:
    explicit OptionalAccessError(const std::string& message)
        : std::runtime_error(message) {}
};

class OptionalOperationError : public std::runtime_error {
public:
    explicit OptionalOperationError(const std::string& message)
        : std::runtime_error(message) {}
};

template <typename T>
concept Copyable = std::is_copy_constructible_v<std::remove_cvref_t<T>>;

template <typename T>
concept Movable = std::is_move_constructible_v<std::remove_cvref_t<T>>;

/**
 * @brief A thread-safe optional wrapper with enhanced functionality.
 *
 * This class provides a wrapper around `std::optional` to represent an optional
 * value that may or may not be present. It supports basic operations such as
 * accessing the contained value, resetting, and applying transformations.
 * This implementation adds thread safety, improved error handling, and
 * optimization.
 *
 * @tparam T The type of the contained value.
 */
template <typename T>
class Optional {
private:
    mutable std::shared_mutex mutex_;
    std::optional<T> storage_;

    void check_value() const {
        if (!storage_.has_value()) {
            throw OptionalAccessError("Optional has no value");
        }
    }

public:
    /**
     * @brief Default constructor.
     *
     * Constructs an empty `Optional` object.
     */
    constexpr Optional() noexcept = default;

    /**
     * @brief Constructor with std::nullopt_t.
     *
     * Constructs an empty `Optional` object using std::nullopt.
     *
     * @param nullopt A nullopt_t instance.
     */
    constexpr Optional(std::nullopt_t) noexcept : storage_(std::nullopt) {}

    /**
     * @brief Constructor with a value.
     *
     * Constructs an `Optional` object containing the given value.
     *
     * @param value The value to be contained.
     */
    template <typename U>
        requires std::convertible_to<U, T>
    explicit Optional(U&& value) noexcept(std::is_nothrow_constructible_v<T, U>)
        : storage_(std::forward<U>(value)) {}

    /**
     * @brief Copy constructor.
     *
     * Constructs an `Optional` object as a copy of another `Optional` object.
     *
     * @param other The other `Optional` object to copy from.
     */
    Optional(const Optional& other) {
        std::shared_lock lock(other.mutex_);
        storage_ = other.storage_;
    }

    /**
     * @brief Move constructor.
     *
     * Constructs an `Optional` object by moving from another `Optional` object.
     * The moved-from object will be empty after the move.
     *
     * @param other The other `Optional` object to move from.
     */
    Optional(Optional&& other) noexcept(
        std::is_nothrow_move_constructible_v<T>) {
        std::unique_lock lock(other.mutex_);
        storage_ = std::move(other.storage_);
        other.storage_.reset();
    }

    /**
     * @brief Destructor.
     *
     * Destroys the `Optional` object.
     */
    ~Optional() = default;

    /**
     * @brief Assignment operator with std::nullopt_t.
     *
     * Assigns an empty state to the `Optional` object.
     *
     * @param nullopt A nullopt_t instance.
     * @return A reference to this `Optional` object.
     */
    Optional& operator=(std::nullopt_t) noexcept {
        std::unique_lock lock(mutex_);
        storage_ = std::nullopt;
        return *this;
    }

    /**
     * @brief Copy assignment operator.
     *
     * Assigns the value of another `Optional` object to this `Optional` object.
     *
     * @param other The other `Optional` object to copy from.
     * @return A reference to this `Optional` object.
     */
    Optional& operator=(const Optional& other) {
        if (this != &other) {
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::shared_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);
            storage_ = other.storage_;
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     *
     * Assigns the value of another `Optional` object to this `Optional` object
     * by moving. The moved-from object will be empty after the move.
     *
     * @param other The other `Optional` object to move from.
     * @return A reference to this `Optional` object.
     */
    Optional& operator=(Optional&& other) noexcept(
        std::is_nothrow_move_assignable_v<T>) {
        if (this != &other) {
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::unique_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);
            storage_ = std::move(other.storage_);
            other.storage_.reset();
        }
        return *this;
    }

    /**
     * @brief Assignment operator with a value.
     *
     * Assigns a new value to the `Optional` object.
     *
     * @param value The new value to assign.
     * @return A reference to this `Optional` object.
     */
    template <typename U>
        requires std::convertible_to<U, T>
    Optional& operator=(U&& value) {
        std::unique_lock lock(mutex_);
        try {
            storage_ = std::forward<U>(value);
        } catch (const std::exception& e) {
            throw OptionalOperationError(std::string("Assignment failed: ") +
                                         e.what());
        }
        return *this;
    }

    /**
     * @brief Constructs a new value in the `Optional` object.
     *
     * Constructs a new value in-place within the `Optional` object using the
     * given arguments.
     *
     * @tparam Args The types of the arguments to forward to the constructor of
     * T.
     * @param args The arguments to forward.
     * @return A reference to the newly constructed value.
     * @throws OptionalOperationError if the construction fails
     */
    template <typename... Args>
    T& emplace(Args&&... args) {
        std::unique_lock lock(mutex_);
        try {
            storage_.emplace(std::forward<Args>(args)...);
            return *storage_;
        } catch (const std::exception& e) {
            throw OptionalOperationError(
                std::string("Emplace operation failed: ") + e.what());
        }
    }

    /**
     * @brief Checks if the `Optional` object contains a value.
     *
     * @return True if the `Optional` object contains a value, false otherwise.
     */
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        std::shared_lock lock(mutex_);
        return storage_.has_value();
    }

    /**
     * @brief Checks if the `Optional` object contains a value.
     *
     * Thread-safe method to check if optional has a value.
     *
     * @return True if the `Optional` object contains a value, false otherwise.
     */
    [[nodiscard]] bool has_value() const noexcept {
        std::shared_lock lock(mutex_);
        return storage_.has_value();
    }

    /**
     * @brief Dereference operator for lvalue `Optional`.
     *
     * Accesses the contained value.
     *
     * @return A reference to the contained value.
     * @throw OptionalAccessError if the `Optional` object is empty.
     */
    T& operator*() & {
        std::shared_lock lock(mutex_);
        check_value();
        return *storage_;
    }

    /**
     * @brief Dereference operator for const lvalue `Optional`.
     *
     * Accesses the contained value.
     *
     * @return A const reference to the contained value.
     * @throw OptionalAccessError if the `Optional` object is empty.
     */
    const T& operator*() const& {
        std::shared_lock lock(mutex_);
        check_value();
        return *storage_;
    }

    /**
     * @brief Dereference operator for rvalue `Optional`.
     *
     * Accesses the contained value and moves it.
     *
     * @return An rvalue reference to the contained value.
     * @throw OptionalAccessError if the `Optional` object is empty.
     */
    T&& operator*() && {
        std::unique_lock lock(mutex_);
        check_value();
        return std::move(*storage_);
    }

    /**
     * @brief Member access operator for lvalue `Optional`.
     *
     * Accesses the contained value using the arrow operator.
     *
     * @return A pointer to the contained value.
     * @throw OptionalAccessError if the `Optional` object is empty.
     */
    T* operator->() {
        std::shared_lock lock(mutex_);
        check_value();
        return &(*storage_);
    }

    /**
     * @brief Member access operator for const lvalue `Optional`.
     *
     * Accesses the contained value using the arrow operator.
     *
     * @return A const pointer to the contained value.
     * @throw OptionalAccessError if the `Optional` object is empty.
     */
    const T* operator->() const {
        std::shared_lock lock(mutex_);
        check_value();
        return &(*storage_);
    }

    /**
     * @brief Accesses the contained value.
     *
     * @return A reference to the contained value.
     * @throw OptionalAccessError if the `Optional` object is empty.
     */
    T& value() & {
        std::shared_lock lock(mutex_);
        check_value();
        return *storage_;
    }

    /**
     * @brief Accesses the contained value for const lvalue `Optional`.
     *
     * @return A const reference to the contained value.
     * @throw OptionalAccessError if the `Optional` object is empty.
     */
    const T& value() const& {
        std::shared_lock lock(mutex_);
        check_value();
        return *storage_;
    }

    /**
     * @brief Accesses the contained value and moves it.
     *
     * @return An rvalue reference to the contained value.
     * @throw OptionalAccessError if the `Optional` object is empty.
     */
    T&& value() && {
        std::unique_lock lock(mutex_);
        check_value();
        return std::move(*storage_);
    }

    /**
     * @brief Returns the contained value or a default value.
     *
     * If the `Optional` object contains a value, it returns that value.
     * Otherwise, it returns the provided default value.
     *
     * @tparam U The type of the default value.
     * @param default_value The default value to return if the `Optional` is
     * empty.
     * @return The contained value if present, otherwise the default value.
     */
    template <typename U>
    T value_or(U&& default_value) const& {
        std::shared_lock lock(mutex_);
        return storage_.has_value()
                   ? *storage_
                   : static_cast<T>(std::forward<U>(default_value));
    }

    /**
     * @brief Returns the contained value or a default value (rvalue version).
     *
     * If the `Optional` object contains a value, it returns that value.
     * Otherwise, it returns the provided default value.
     *
     * @tparam U The type of the default value.
     * @param default_value The default value to return if the `Optional` is
     * empty.
     * @return The contained value if present, otherwise the default value.
     */
    template <typename U>
    T value_or(U&& default_value) && {
        std::unique_lock lock(mutex_);
        return storage_.has_value()
                   ? std::move(*storage_)
                   : static_cast<T>(std::forward<U>(default_value));
    }

    /**
     * @brief Resets the `Optional` object to an empty state.
     *
     * This function clears the contained value, if any, leaving the `Optional`
     * object in an empty state.
     */
    void reset() noexcept {
        std::unique_lock lock(mutex_);
        storage_.reset();
    }

    /**
     * @brief Three-way comparison operator.
     *
     * Thread-safe comparison of two `Optional` objects.
     *
     * @param other The other `Optional` object to compare to.
     * @return A three-way comparison result.
     */
    auto operator<=>(const Optional& other) const {
        std::shared_lock lock1(mutex_, std::defer_lock);
        std::shared_lock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);

        if (storage_.has_value() && other.storage_.has_value()) {
            return *storage_ <=> *other.storage_;
        } else if (storage_.has_value()) {
            return std::strong_ordering::greater;
        } else if (other.storage_.has_value()) {
            return std::strong_ordering::less;
        } else {
            return std::strong_ordering::equal;
        }
    }

    /**
     * @brief Equality comparison operator.
     *
     * Thread-safe comparison of two `Optional` objects.
     *
     * @param other The other `Optional` object to compare to.
     * @return True if both have values and they're equal, or if both are empty.
     */
    bool operator==(const Optional& other) const {
        std::shared_lock lock1(mutex_, std::defer_lock);
        std::shared_lock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);

        if (storage_.has_value() && other.storage_.has_value()) {
            return *storage_ == *other.storage_;
        }
        return storage_.has_value() == other.storage_.has_value();
    }

    /**
     * @brief Comparison with std::nullopt_t.
     *
     * Checks if the `Optional` object is equal to `std::nullopt`.
     *
     * @param nullopt A nullopt_t instance.
     * @return True if the `Optional` object is empty, false otherwise.
     */
    bool operator==(std::nullopt_t) const noexcept {
        std::shared_lock lock(mutex_);
        return !storage_.has_value();
    }

    /**
     * @brief Comparison with std::nullopt_t (three-way comparison).
     *
     * Compares the `Optional` object with `std::nullopt`.
     *
     * @param nullopt A nullopt_t instance.
     * @return A three-way comparison result.
     */
    auto operator<=>(std::nullopt_t) const noexcept {
        std::shared_lock lock(mutex_);
        return storage_.has_value() ? std::strong_ordering::greater
                                    : std::strong_ordering::equal;
    }

    /**
     * @brief Applies a function to the contained value, if present.
     *
     * If the `Optional` object contains a value, applies the function to that
     * value and returns a new `Optional` object with the result. Otherwise,
     * returns an empty `Optional`.
     *
     * @tparam F The type of the function.
     * @param f The function to apply.
     * @return An `Optional` object containing the result of applying the
     * function, or an empty `Optional`.
     */
    template <typename F>
    auto map(F&& f) const -> Optional<std::invoke_result_t<F, T>> {
        using ReturnType = std::invoke_result_t<F, T>;

        std::shared_lock lock(mutex_);
        if (!storage_.has_value()) {
            return Optional<ReturnType>{};
        }

        try {
            return Optional<ReturnType>(
                std::invoke(std::forward<F>(f), *storage_));
        } catch (const std::exception& e) {
            throw OptionalOperationError(std::string("Map operation failed: ") +
                                         e.what());
        }
    }

    /**
     * @brief Applies a function to the contained value, if present.
     *
     * If the `Optional` object contains a value, applies the function to that
     * value and returns the result.
     *
     * @tparam F The type of the function.
     * @param f The function to apply.
     * @return The result of applying the function, or a default-constructed
     * value if the `Optional` is empty.
     */
    template <typename F>
    auto and_then(F&& f) const -> std::invoke_result_t<F, T> {
        std::shared_lock lock(mutex_);
        if (!storage_.has_value()) {
            return std::invoke_result_t<F, T>();
        }

        try {
            return std::invoke(std::forward<F>(f), *storage_);
        } catch (const std::exception& e) {
            throw OptionalOperationError(
                std::string("And_then operation failed: ") + e.what());
        }
    }

    /**
     * @brief Applies a function to the contained value and returns a new
     * `Optional` with the result.
     *
     * This function is an alias for `map`.
     *
     * @tparam F The type of the function.
     * @param f The function to apply.
     * @return An `Optional` object containing the result of applying the
     * function, or an empty `Optional`.
     */
    template <typename F>
    auto transform(F&& f) const -> Optional<std::invoke_result_t<F, T>> {
        return map(std::forward<F>(f));
    }

    /**
     * @brief Returns the contained value or invokes a function to generate a
     * default value.
     *
     * If the `Optional` object contains a value, returns that value.
     * Otherwise, invokes the provided function to generate a default value.
     *
     * @tparam F The type of the function.
     * @param f The function to invoke if the `Optional` is empty.
     * @return The contained value if present, otherwise the result of invoking
     * the function.
     */
    template <typename F>
    auto or_else(F&& f) const -> T {
        std::shared_lock lock(mutex_);
        if (storage_.has_value()) {
            return *storage_;
        }

        try {
            return std::invoke(std::forward<F>(f));
        } catch (const std::exception& e) {
            throw OptionalOperationError(
                std::string("Or_else operation failed: ") + e.what());
        }
    }

    /**
     * @brief Executes the provided function if the optional contains a value
     *
     * A convenience method for side effects when a value is present.
     *
     * @tparam F The type of the function taking T as parameter
     * @param f The function to execute
     * @return Reference to this optional
     */
    template <typename F>
    Optional& if_has_value(F&& f) {
        std::shared_lock lock(mutex_);
        if (storage_.has_value()) {
            try {
                std::invoke(std::forward<F>(f), *storage_);
            } catch (const std::exception& e) {
                throw OptionalOperationError(
                    std::string("If_has_value operation failed: ") + e.what());
            }
        }
        return *this;
    }
};

/**
 * @brief Creates an Optional object containing the given value.
 *
 * This is a convenience function for creating Optional objects.
 *
 * @tparam T The type of the value.
 * @param value The value to contain.
 * @return An Optional object containing the value.
 */
template <typename T>
constexpr auto make_optional(T&& value) {
    return Optional<std::decay_t<T>>(std::forward<T>(value));
}

/**
 * @brief Creates an Optional object containing a value constructed in-place.
 *
 * This is a convenience function for creating Optional objects with in-place
 * construction.
 *
 * @tparam T The type of the value.
 * @tparam Args The types of the arguments to forward to the constructor of T.
 * @param args The arguments to forward.
 * @return An Optional object containing the constructed value.
 */
template <typename T, typename... Args>
constexpr auto make_optional(Args&&... args) {
    Optional<T> result;
    result.emplace(std::forward<Args>(args)...);
    return result;
}

}  // namespace atom::type

#endif  // ATOM_TYPE_OPTIONAL_HPP

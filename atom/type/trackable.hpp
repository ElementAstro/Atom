/*
 * trackable.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-10

Description: Trackable Object (Optimized with C++20 features)

**************************************************/

#ifndef ATOM_TYPE_TRACKABLE_HPP
#define ATOM_TYPE_TRACKABLE_HPP

#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#ifdef ATOM_USE_BOOST
#include <boost/core/demangle.hpp>
#include <boost/type_traits.hpp>
#endif

#include "atom/error/exception.hpp"
#include "atom/meta/abi.hpp"

/**
 * @brief A class template for creating trackable objects that notify observers
 * when their value changes.
 *
 * @tparam T The type of the value being tracked.
 */
template <typename T>
class Trackable {
public:
    using value_type = T;

    /**
     * @brief Constructor to initialize the trackable object with an initial
     * value.
     *
     * @param initialValue The initial value of the trackable object.
     */
    explicit Trackable(T initialValue) : value_(std::move(initialValue)) {}

    /**
     * @brief Subscribe a callback function to be called when the value changes.
     *
     * @param onChange The callback function to be called when the value
     * changes. It takes two const references: the old value and the new value.
     */
    void subscribe(std::function<void(const T&, const T&)> onChange) {
        std::scoped_lock lock(mutex_);
        observers_.emplace_back(std::move(onChange));
    }

    /**
     * @brief Set a callback that will be triggered when the value changes.
     *
     * @param onChange The callback function to be called when the value
     * changes. It takes the new value as an argument.
     */
    void setOnChangeCallback(std::function<void(const T&)> onChange) {
        std::scoped_lock lock(mutex_);
        onChangeCallback_ = std::move(onChange);
    }

    /**
     * @brief Unsubscribe all observer functions.
     */
    void unsubscribeAll() {
        std::scoped_lock lock(mutex_);
        observers_.clear();
    }

    /**
     * @brief Checks if there are any subscribers.
     *
     * @return true if there are subscribers, false otherwise.
     */
    [[nodiscard]] auto hasSubscribers() const -> bool {
        std::scoped_lock lock(mutex_);
        return !observers_.empty();
    }

    /**
     * @brief Get the current value of the trackable object.
     *
     * @return const T& A const reference to the current value.
     */
    [[nodiscard]] auto get() const -> const T& {
        std::scoped_lock lock(mutex_);
        return value_;
    }

    /**
     * @brief Get the demangled type name of the stored value.
     *
     * @return std::string The demangled type name of the value.
     */
    [[nodiscard]] auto getTypeName() const -> std::string {
#ifdef ATOM_USE_BOOST
        return boost::core::demangle(typeid(T).name());
#else
        return atom::meta::DemangleHelper::demangleType<T>();
#endif
    }

    /**
     * @brief Overloaded assignment operator to update the value and notify
     * observers.
     *
     * @param newValue The new value to be assigned.
     * @return Trackable& Reference to the trackable object.
     */
    auto operator=(T newValue) -> Trackable& {
        std::scoped_lock lock(mutex_);
        if (value_ != newValue) {
            T oldValue = std::exchange(value_, std::move(newValue));
            if (!notifyDeferred_) {
                notifyObservers(oldValue, value_);
            } else {
                lastOldValue_ = std::move(oldValue);
            }
        }
        return *this;
    }

    /**
     * @brief Arithmetic compound assignment operators.
     *
     * @param rhs The right-hand side value for the operation.
     * @return Trackable& Reference to the trackable object.
     */
    auto operator+=(const T& rhs) -> Trackable& {
        return applyOperation(rhs, std::plus<>{});
    }

    auto operator-=(const T& rhs) -> Trackable& {
        return applyOperation(rhs, std::minus<>{});
    }

    auto operator*=(const T& rhs) -> Trackable& {
        return applyOperation(rhs, std::multiplies<>{});
    }

    auto operator/=(const T& rhs) -> Trackable& {
        return applyOperation(rhs, std::divides<>{});
    }

    /**
     * @brief Conversion operator to convert the trackable object to its value
     * type.
     *
     * @return T The value of the trackable object.
     */
    explicit operator T() const { return get(); }

    /**
     * @brief Control whether notifications are deferred or not.
     *
     * @param defer If true, notifications will be deferred until
     * deferNotifications(false) is called.
     */
    void deferNotifications(bool defer) {
        std::scoped_lock lock(mutex_);
        notifyDeferred_ = defer;
        if (!defer && lastOldValue_.has_value()) {
            notifyObservers(*lastOldValue_, value_);
            lastOldValue_.reset();
        }
    }

    /**
     * @brief A scope-based notification deferrer.
     */
    class ScopedDefer {
    public:
        explicit ScopedDefer(Trackable* parent) : parent_(parent) {}
        ~ScopedDefer() { parent_->deferNotifications(false); }

        // Non-copyable
        ScopedDefer(const ScopedDefer&) = delete;
        ScopedDefer& operator=(const ScopedDefer&) = delete;

        // Movable
        ScopedDefer(ScopedDefer&& other) noexcept : parent_(other.parent_) {
            other.parent_ = nullptr;
        }
        ScopedDefer& operator=(ScopedDefer&& other) noexcept {
            if (this != &other) {
                if (parent_)
                    parent_->deferNotifications(false);
                parent_ = other.parent_;
                other.parent_ = nullptr;
            }
            return *this;
        }

    private:
        Trackable* parent_;
    };

    [[nodiscard]] auto deferScoped() {
        deferNotifications(true);
        return ScopedDefer(this);
    }

private:
    T value_;  ///< The stored value.
    std::vector<std::function<void(const T&, const T&)>>
        observers_;               ///< List of observer functions.
    mutable std::mutex mutex_;    ///< Mutex for thread safety.
    bool notifyDeferred_{false};  ///< Flag to control deferred notifications.
    std::optional<T>
        lastOldValue_;  ///< Last old value for deferred notifications.
    std::function<void(const T&)>
        onChangeCallback_;  ///< Callback for value changes.

    /**
     * @brief Notifies all observers about the value change.
     *
     * @param oldVal The old value.
     * @param newVal The new value.
     */
    void notifyObservers(const T& oldVal, const T& newVal) {
        // Create copies of the observers to avoid holding the lock during
        // callbacks
        std::vector<std::function<void(const T&, const T&)>> localObservers;
        std::function<void(const T&)> localOnChangeCallback;
        {
            std::scoped_lock lock(mutex_);
            localObservers = observers_;
            localOnChangeCallback = onChangeCallback_;
        }

        for (const auto& observer : localObservers) {
            try {
                observer(oldVal, newVal);
            } catch (const std::exception& e) {
                THROW_EXCEPTION("Exception in observer: ", e.what());
            } catch (...) {
                THROW_EXCEPTION("Unknown exception in observer.");
            }
        }

        if (localOnChangeCallback) {
            try {
                localOnChangeCallback(newVal);
            } catch (const std::exception& e) {
                THROW_EXCEPTION("Exception in onChangeCallback: ", e.what());
            } catch (...) {
                THROW_EXCEPTION("Unknown exception in onChangeCallback.");
            }
        }
    }

    /**
     * @brief Applies an arithmetic operation and notifies observers.
     *
     * @param rhs The right-hand side operand.
     * @param operation The operation to apply.
     * @return Trackable& Reference to the trackable object.
     */
    template <typename Operation>
    auto applyOperation(const T& rhs, Operation operation) -> Trackable& {
        std::scoped_lock lock(mutex_);
        T newValue = operation(value_, rhs);
        if (value_ != newValue) {
            T oldValue = std::exchange(value_, std::move(newValue));
            if (!notifyDeferred_) {
                notifyObservers(oldValue, value_);
            } else {
                lastOldValue_ = std::move(oldValue);
            }
        }
        return *this;
    }
};

#endif  // ATOM_TYPE_TRACKABLE_HPP
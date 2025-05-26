#ifndef ATOM_META_PROPERTY_HPP
#define ATOM_META_PROPERTY_HPP

#include <functional>
#include <iostream>
#include <mutex>
#include <shared_mutex>

#include "atom/error/exception.hpp"

namespace atom::meta {

/**
 * @brief A template class that encapsulates a property with optional getter,
 * setter, and onChange callback.
 *
 * @tparam T The type of the property value.
 */
template <typename T>
class Property {
private:
    mutable T value_{};
    mutable bool hasValue_ = false;
    std::function<T()> getter_;
    std::function<void(const T&)> setter_;
    std::function<void(const T&)> onChange_;
    mutable std::shared_mutex mutex_;

public:
    /**
     * @brief Default constructor.
     */
    Property() = default;

    /**
     * @brief Constructor that initializes the property with a getter function.
     *
     * @param get The getter function.
     */
    explicit Property(std::function<T()> get) : getter_(std::move(get)) {}

    /**
     * @brief Constructor that initializes the property with a getter and setter
     * function.
     *
     * @param get The getter function.
     * @param set The setter function.
     */
    Property(std::function<T()> get, std::function<void(const T&)> set)
        : getter_(std::move(get)), setter_(std::move(set)) {}

    /**
     * @brief Constructor that initializes the property with a default value.
     *
     * @param defaultValue The default value of the property.
     */
    explicit Property(const T& defaultValue)
        : value_(defaultValue), hasValue_(true) {}

    /**
     * @brief Destructor.
     */
    ~Property() = default;

    /**
     * @brief Copy constructor.
     *
     * @param other The other Property object to copy from.
     */
    Property(const Property& other) {
        std::shared_lock lock(other.mutex_);
        value_ = other.value_;
        hasValue_ = other.hasValue_;
        getter_ = other.getter_;
        setter_ = other.setter_;
        onChange_ = other.onChange_;
    }

    /**
     * @brief Copy assignment operator.
     *
     * @param other The other Property object to copy from.
     * @return Property& A reference to this Property object.
     */
    auto operator=(const Property& other) -> Property& {
        if (this != &other) {
            std::shared_lock otherLock(other.mutex_);
            std::unique_lock thisLock(mutex_);
            value_ = other.value_;
            hasValue_ = other.hasValue_;
            getter_ = other.getter_;
            setter_ = other.setter_;
            onChange_ = other.onChange_;
        }
        return *this;
    }

    /**
     * @brief Move constructor.
     *
     * @param other The other Property object to move from.
     */
    Property(Property&& other) noexcept {
        std::unique_lock lock(other.mutex_);
        value_ = std::move(other.value_);
        hasValue_ = other.hasValue_;
        getter_ = std::move(other.getter_);
        setter_ = std::move(other.setter_);
        onChange_ = std::move(other.onChange_);
        other.hasValue_ = false;
    }

    /**
     * @brief Move assignment operator.
     *
     * @param other The other Property object to move from.
     * @return Property& A reference to this Property object.
     */
    auto operator=(Property&& other) noexcept -> Property& {
        if (this != &other) {
            std::unique_lock thisLock(mutex_);
            std::unique_lock otherLock(other.mutex_);
            value_ = std::move(other.value_);
            hasValue_ = other.hasValue_;
            getter_ = std::move(other.getter_);
            setter_ = std::move(other.setter_);
            onChange_ = std::move(other.onChange_);
            other.hasValue_ = false;
        }
        return *this;
    }

    /**
     * @brief Conversion operator to the underlying type T.
     *
     * @return T The value of the property.
     * @throws std::invalid_argument if neither value nor getter is defined.
     */
    explicit operator T() const {
        std::shared_lock lock(mutex_);
        if (getter_) {
            return getter_();
        }
        if (hasValue_) {
            return value_;
        }
        THROW_INVALID_ARGUMENT("Property has no value or getter defined");
    }

    /**
     * @brief Gets the value of the property.
     *
     * @return T The value of the property.
     * @throws std::invalid_argument if neither value nor getter is defined.
     */
    [[nodiscard]] auto get() const -> T { return static_cast<T>(*this); }

    /**
     * @brief Assignment operator for the underlying type T.
     *
     * @param newValue The new value to set.
     * @return Property& A reference to this Property object.
     */
    auto operator=(const T& newValue) -> Property& {
        set(newValue);
        return *this;
    }

    /**
     * @brief Sets the value of the property.
     *
     * @param newValue The new value to set.
     */
    void set(const T& newValue) {
        {
            std::unique_lock lock(mutex_);
            if (setter_) {
                setter_(newValue);
            } else {
                value_ = newValue;
                hasValue_ = true;
            }
        }
        notifyChange(newValue);
    }

    /**
     * @brief Sets the property to readonly by removing the setter function.
     */
    void makeReadonly() {
        std::unique_lock lock(mutex_);
        setter_ = nullptr;
    }

    /**
     * @brief Sets the property to writeonly by removing the getter function.
     */
    void makeWriteonly() {
        std::unique_lock lock(mutex_);
        getter_ = nullptr;
    }

    /**
     * @brief Removes both getter and setter functions.
     */
    void clear() {
        std::unique_lock lock(mutex_);
        getter_ = nullptr;
        setter_ = nullptr;
        hasValue_ = false;
    }

    /**
     * @brief Sets the onChange callback function.
     *
     * @param callback The onChange callback function.
     */
    void setOnChange(std::function<void(const T&)> callback) {
        std::unique_lock lock(mutex_);
        onChange_ = std::move(callback);
    }

    /**
     * @brief Checks if the property has a value.
     *
     * @return bool True if the property has a value, false otherwise.
     */
    [[nodiscard]] auto hasValue() const -> bool {
        std::shared_lock lock(mutex_);
        return hasValue_ || getter_;
    }

    /**
     * @brief Checks if the property is readonly.
     *
     * @return bool True if the property is readonly, false otherwise.
     */
    [[nodiscard]] auto isReadonly() const -> bool {
        std::shared_lock lock(mutex_);
        return !setter_;
    }

    /**
     * @brief Checks if the property is writeonly.
     *
     * @return bool True if the property is writeonly, false otherwise.
     */
    [[nodiscard]] auto isWriteonly() const -> bool {
        std::shared_lock lock(mutex_);
        return !getter_ && !hasValue_;
    }

    /**
     * @brief Stream output operator for the Property class.
     *
     * @param outputStream The output stream.
     * @param prop The Property object to output.
     * @return std::ostream& The output stream.
     */
    friend auto operator<<(std::ostream& outputStream, const Property& prop)
        -> std::ostream& {
        try {
            outputStream << static_cast<T>(prop);
        } catch (const std::exception&) {
            outputStream << "[Property: no value]";
        }
        return outputStream;
    }

    /**
     * @brief Three-way comparison operator.
     *
     * @param other The other value to compare with.
     * @return auto The result of the comparison.
     */
    auto operator<=>(const T& other) const {
        return static_cast<T>(*this) <=> other;
    }

    /**
     * @brief Equality comparison operator.
     *
     * @param other The other value to compare with.
     * @return bool True if equal, false otherwise.
     */
    auto operator==(const T& other) const -> bool {
        try {
            return static_cast<T>(*this) == other;
        } catch (const std::exception&) {
            return false;
        }
    }

    /**
     * @brief Inequality comparison operator.
     *
     * @param other The other value to compare with.
     * @return bool True if not equal, false otherwise.
     */
    auto operator!=(const T& other) const -> bool { return !(*this == other); }

    /**
     * @brief Addition assignment operator.
     *
     * @param other The other value to add.
     * @return Property& A reference to this Property object.
     */
    auto operator+=(const T& other) -> Property& {
        *this = static_cast<T>(*this) + other;
        return *this;
    }

    /**
     * @brief Subtraction assignment operator.
     *
     * @param other The other value to subtract.
     * @return Property& A reference to this Property object.
     */
    auto operator-=(const T& other) -> Property& {
        *this = static_cast<T>(*this) - other;
        return *this;
    }

    /**
     * @brief Multiplication assignment operator.
     *
     * @param other The other value to multiply.
     * @return Property& A reference to this Property object.
     */
    auto operator*=(const T& other) -> Property& {
        *this = static_cast<T>(*this) * other;
        return *this;
    }

    /**
     * @brief Division assignment operator.
     *
     * @param other The other value to divide.
     * @return Property& A reference to this Property object.
     */
    auto operator/=(const T& other) -> Property& {
        *this = static_cast<T>(*this) / other;
        return *this;
    }

    /**
     * @brief Modulus assignment operator.
     *
     * @param other The other value to modulus.
     * @return Property& A reference to this Property object.
     */
    template <typename U = T>
    auto operator%=(const T& other)
        -> std::enable_if_t<std::is_integral_v<U>, Property&> {
        *this = static_cast<T>(*this) % other;
        return *this;
    }

private:
    /**
     * @brief Notifies listeners of a change in the property value.
     *
     * @param newValue The new value of the property.
     */
    void notifyChange(const T& newValue) const {
        std::shared_lock lock(mutex_);
        if (onChange_) {
            onChange_(newValue);
        }
    }
};

/**
 * @brief Creates a property with getter and setter functions.
 *
 * @tparam T The type of the property value.
 * @param getter The getter function.
 * @param setter The setter function.
 * @return Property<T> The created property.
 */
template <typename T>
auto makeProperty(std::function<T()> getter,
                  std::function<void(const T&)> setter) -> Property<T> {
    return Property<T>(std::move(getter), std::move(setter));
}

/**
 * @brief Creates a readonly property with a getter function.
 *
 * @tparam T The type of the property value.
 * @param getter The getter function.
 * @return Property<T> The created readonly property.
 */
template <typename T>
auto makeReadonlyProperty(std::function<T()> getter) -> Property<T> {
    return Property<T>(std::move(getter));
}

/**
 * @brief Creates a property with an initial value.
 *
 * @tparam T The type of the property value.
 * @param value The initial value.
 * @return Property<T> The created property.
 */
template <typename T>
auto makeValueProperty(const T& value) -> Property<T> {
    return Property<T>(value);
}

}  // namespace atom::meta

/**
 * @brief Macro to define a read-write property.
 *
 * @param Type The type of the property.
 * @param Name The name of the property.
 */
#define DEFINE_RW_PROPERTY(Type, Name)        \
private:                                      \
    Type Name##_;                             \
                                              \
public:                                       \
    atom::meta::Property<Type> Name{          \
        [this]() -> Type { return Name##_; }, \
        [this](const Type& value) { Name##_ = value; }};

/**
 * @brief Macro to define a read-only property.
 *
 * @param Type The type of the property.
 * @param Name The name of the property.
 */
#define DEFINE_RO_PROPERTY(Type, Name) \
private:                               \
    Type Name##_;                      \
                                       \
public:                                \
    atom::meta::Property<Type> Name{[this]() -> Type { return Name##_; }};

/**
 * @brief Macro to define a write-only property.
 *
 * @param Type The type of the property.
 * @param Name The name of the property.
 */
#define DEFINE_WO_PROPERTY(Type, Name) \
private:                               \
    Type Name##_;                      \
                                       \
public:                                \
    atom::meta::Property<Type> Name{   \
        nullptr, [this](const Type& value) { Name##_ = value; }};

#endif  // ATOM_META_PROPERTY_HPP

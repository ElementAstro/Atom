#ifndef ATOM_META_PROPERTY_HPP
#define ATOM_META_PROPERTY_HPP

#include <concepts>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

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
    mutable std::unordered_map<std::string, T> cache_;
    mutable std::shared_mutex cacheMutex_;

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
    friend auto operator<<(std::ostream& outputStream,
                           const Property& prop) -> std::ostream& {
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
     * @brief Atomically read-modify-write the property value.
     *
     * The whole read-compute-store cycle happens under one exclusive lock,
     * so concurrent modify() calls never lose updates (unlike separate
     * get()/set() pairs).
     *
     * @param mutator Callable receiving a mutable reference to the value.
     * @return Property& A reference to this Property object.
     */
    template <typename F>
        requires std::invocable<F, T&>
    auto modify(F&& mutator) -> Property& {
        T updated;
        {
            std::unique_lock lock(mutex_);
            T current = getter_   ? getter_()
                        : hasValue_ ? value_
                                    : T{};
            std::forward<F>(mutator)(current);
            if (setter_) {
                setter_(current);
            } else {
                value_ = current;
                hasValue_ = true;
            }
            updated = std::move(current);
        }
        notifyChange(updated);
        return *this;
    }

    /**
     * @brief Addition assignment operator (atomic read-modify-write).
     *
     * @param other The other value to add.
     * @return Property& A reference to this Property object.
     */
    auto operator+=(const T& other) -> Property& {
        return modify([&](T& value) { value = value + other; });
    }

    /**
     * @brief Subtraction assignment operator (atomic read-modify-write).
     *
     * @param other The other value to subtract.
     * @return Property& A reference to this Property object.
     */
    auto operator-=(const T& other) -> Property& {
        return modify([&](T& value) { value = value - other; });
    }

    /**
     * @brief Multiplication assignment operator (atomic read-modify-write).
     *
     * @param other The other value to multiply.
     * @return Property& A reference to this Property object.
     */
    auto operator*=(const T& other) -> Property& {
        return modify([&](T& value) { value = value * other; });
    }

    /**
     * @brief Division assignment operator (atomic read-modify-write).
     *
     * @param other The other value to divide.
     * @return Property& A reference to this Property object.
     */
    auto operator/=(const T& other) -> Property& {
        return modify([&](T& value) { value = value / other; });
    }

    /**
     * @brief Modulus assignment operator (atomic read-modify-write).
     *
     * @param other The other value to modulus.
     * @return Property& A reference to this Property object.
     */
    auto operator%=(const T& other) -> Property&
        requires requires(const T& lhs, const T& rhs) { lhs % rhs; }
    {
        return modify([&](T& value) { value = value % other; });
    }

    /**
     * @brief Asynchronously gets the value of the property.
     *
     * @return std::future<T> A future holding the property value.
     */
    [[nodiscard]] auto asyncGet() const -> std::future<T> {
        return std::async(std::launch::async, [this]() { return get(); });
    }

    /**
     * @brief Asynchronously sets the value of the property.
     *
     * @param newValue The new value to set.
     * @return std::future<void> A future that completes once the value is set.
     */
    auto asyncSet(const T& newValue) -> std::future<void> {
        return std::async(std::launch::async,
                          [this, newValue]() { set(newValue); });
    }

    /**
     * @brief Caches a value under the given key.
     *
     * @param key The cache key.
     * @param value The value to cache.
     */
    void cacheValue(const std::string& key, const T& value) const {
        std::unique_lock lock(cacheMutex_);
        cache_.insert_or_assign(key, value);
    }

    /**
     * @brief Retrieves a cached value by key.
     *
     * @param key The cache key.
     * @return std::optional<T> The cached value, or std::nullopt if not found.
     */
    [[nodiscard]] auto getCachedValue(const std::string& key) const
        -> std::optional<T> {
        std::shared_lock lock(cacheMutex_);
        if (auto it = cache_.find(key); it != cache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief Clears all cached values.
     */
    void clearCache() const {
        std::unique_lock lock(cacheMutex_);
        cache_.clear();
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

//==============================================================================
// C++23 Enhanced Property Utilities
//==============================================================================

/**
 * @brief Concept for property types
 */
template <typename T>
concept PropertyType = std::copy_constructible<T> || std::move_constructible<T>;

/**
 * @brief Concept for observable properties
 */
template <typename T>
concept Observable = requires(T t) {
    { t.addObserver(std::declval<std::function<void()>>()) };
};

/**
 * @brief Property with validation
 */
template <PropertyType T>
class ValidatedProperty : public Property<T> {
    std::function<bool(const T&)> validator_;
    std::string validation_error_;

public:
    using Property<T>::Property;

    ValidatedProperty& withValidator(
        std::function<bool(const T&)> validator,
        std::string_view error_msg = "Validation failed") {
        validator_ = std::move(validator);
        validation_error_ = std::string(error_msg);
        return *this;
    }

    bool trySet(const T& value) {
        if (validator_ && !validator_(value)) {
            return false;
        }
        Property<T>::set(value);
        return true;
    }

    [[nodiscard]] std::string_view getValidationError() const {
        return validation_error_;
    }
};

/**
 * @brief Property with history tracking
 */
template <PropertyType T, std::size_t HistorySize = 10>
class HistoricalProperty : public Property<T> {
    std::array<T, HistorySize> history_;
    std::size_t history_index_ = 0;
    std::size_t history_count_ = 0;

public:
    using Property<T>::Property;

    void set(const T& value) {
        if (history_count_ < HistorySize) {
            history_[history_count_++] = Property<T>::get();
        } else {
            history_[history_index_] = Property<T>::get();
            history_index_ = (history_index_ + 1) % HistorySize;
        }
        Property<T>::set(value);
    }

    [[nodiscard]] std::optional<T> getPrevious(std::size_t steps = 1) const {
        if (steps > history_count_)
            return std::nullopt;
        std::size_t idx = (history_index_ + HistorySize - steps) % HistorySize;
        return history_[idx];
    }

    bool undo() {
        if (auto prev = getPrevious()) {
            Property<T>::set(*prev);
            if (history_count_ > 0)
                --history_count_;
            return true;
        }
        return false;
    }
};

/**
 * @brief Computed property (read-only, derived from other values)
 */
template <PropertyType T>
class ComputedProperty {
    std::function<T()> compute_;
    mutable std::optional<T> cached_;
    mutable bool dirty_ = true;

public:
    explicit ComputedProperty(std::function<T()> compute)
        : compute_(std::move(compute)) {}

    [[nodiscard]] T get() const {
        if (dirty_ || !cached_) {
            cached_ = compute_();
            dirty_ = false;
        }
        return *cached_;
    }

    void invalidate() { dirty_ = true; }

    operator T() const { return get(); }
};

/**
 * @brief Create a computed property
 */
template <typename Func>
auto makeComputed(Func&& compute) {
    using ReturnType = std::invoke_result_t<Func>;
    return ComputedProperty<ReturnType>(std::forward<Func>(compute));
}

/**
 * @brief Property registry for named properties
 */
class PropertyRegistry {
    std::unordered_map<std::string, std::any> properties_;
    mutable std::shared_mutex mutex_;

public:
    template <PropertyType T>
    void registerProperty(std::string_view name, Property<T>* prop) {
        std::unique_lock lock(mutex_);
        properties_[std::string(name)] = prop;
    }

    template <typename T>
    Property<T>* getProperty(std::string_view name) {
        std::shared_lock lock(mutex_);
        auto it = properties_.find(std::string(name));
        if (it != properties_.end()) {
            return std::any_cast<Property<T>*>(it->second);
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<std::string> getPropertyNames() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> names;
        names.reserve(properties_.size());
        for (const auto& [name, _] : properties_) {
            names.push_back(name);
        }
        return names;
    }

    static PropertyRegistry& getInstance() {
        static PropertyRegistry instance;
        return instance;
    }
};

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

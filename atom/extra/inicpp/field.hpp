#ifndef ATOM_EXTRA_INICPP_INIFIELD_HPP
#define ATOM_EXTRA_INICPP_INIFIELD_HPP

#include <optional>
#include <stdexcept>
#include <string>

#include "common.hpp"
#include "convert.hpp"

namespace inicpp {

/**
 * @brief Class representing a field in an INI file.
 */
class IniField {
private:
    small_string value_;

public:
    /**
     * @brief Default constructor.
     */
    IniField() = default;

    /**
     * @brief Construct from a string value.
     * @param value The value to store.
     */
    template <typename StringType>
        requires StringLike<StringType>
    explicit IniField(StringType value) noexcept : value_(value) {}

    /**
     * @brief Copy constructor.
     */
    IniField(const IniField &field) = default;

    /**
     * @brief Move constructor.
     */
    IniField(IniField &&field) noexcept = default;

    /**
     * @brief Destructor.
     */
    ~IniField() = default;

    /**
     * @brief Convert the field to the specified type.
     * @tparam T The target type.
     * @return The converted value.
     * @throws std::invalid_argument if the conversion fails.
     */
    template <typename T>
    [[nodiscard]] T as() const {
        try {
            T result{};
            Convert<T> conv;
            conv.decode(value_, result);
            return result;
        } catch (const std::exception &ex) {
            throw std::invalid_argument("Failed to convert field value '" +
                                        value_ +
                                        "' to requested type: " + ex.what());
        }
    }

    /**
     * @brief Convert the field to the specified type with validation.
     * @tparam T The target type.
     * @return An optional containing the converted value, or nullopt if
     * conversion fails.
     */
    template <typename T>
    [[nodiscard]] std::optional<T> as_optional() const noexcept {
        try {
            return as<T>();
        } catch (const std::exception &) {
            return std::nullopt;
        }
    }

    /**
     * @brief Assign a value of any type to the field.
     * @tparam T The type of the value.
     * @param value The value to assign.
     * @return Reference to this field.
     */
    template <typename T>
    IniField &operator=(const T &value) {
        try {
            Convert<T> conv;
            conv.encode(value, value_);
            return *this;
        } catch (const std::exception &ex) {
            throw std::invalid_argument("Failed to encode value to field: " +
                                        std::string(ex.what()));
        }
    }

    /**
     * @brief Copy assignment operator.
     * @param field The field to copy.
     * @return Reference to this field.
     */
    IniField &operator=(const IniField &field) = default;

    /**
     * @brief Move assignment operator.
     * @param field The field to move from.
     * @return Reference to this field.
     */
    IniField &operator=(IniField &&field) noexcept = default;

    /**
     * @brief Get the raw string value.
     * @return The raw string value of the field.
     */
    [[nodiscard]] std::string_view raw_value() const noexcept { return value_; }

    /**
     * @brief Compare two fields for equality.
     * @param other The other field.
     * @return True if the fields are equal, false otherwise.
     */
    bool operator==(const IniField& other) const noexcept {
        return value_ == other.value_;
    }

    /**
     * @brief Compare two fields for inequality.
     * @param other The other field.
     * @return True if the fields are not equal, false otherwise.
     */
    bool operator!=(const IniField& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Check if field value is empty.
     * @return True if the field value is empty, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept {
        return value_.empty();
    }

    /**
     * @brief Get the size of the field value.
     * @return The size of the field value.
     */
    [[nodiscard]] size_t size() const noexcept {
        return value_.size();
    }

    /**
     * @brief Clear the field value.
     */
    void clear() noexcept {
        value_.clear();
    }
};

#if INICPP_CONFIG_USE_MEMORY_POOL
/**
 * @brief Memory pool for IniField objects.
 */
class IniFieldPool {
private:
    static boost::object_pool<IniField> pool_;

public:
    /**
     * @brief Allocate a new IniField from the pool.
     * @return A new IniField.
     */
    static IniField* allocate() {
        return pool_.construct();
    }

    /**
     * @brief Allocate a new IniField from the pool with an initial value.
     * @param value The initial value.
     * @return A new IniField.
     */
    template <typename StringType>
        requires StringLike<StringType>
    static IniField* allocate(StringType value) {
        return pool_.construct(value);
    }

    /**
     * @brief Free an IniField back to the pool.
     * @param field The field to free.
     */
    static void free(IniField* field) {
        pool_.destroy(field);
    }
};

// 在cpp文件中定义
inline boost::object_pool<IniField> IniFieldPool::pool_;
#endif

}  // namespace inicpp

#endif  // ATOM_EXTRA_INICPP_INIFIELD_HPP

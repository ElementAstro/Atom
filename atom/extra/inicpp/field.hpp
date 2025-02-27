#ifndef ATOM_EXTRA_INICPP_INIFIELD_HPP
#define ATOM_EXTRA_INICPP_INIFIELD_HPP

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include "convert.hpp"

namespace inicpp {

/**
 * @brief Class representing a field in an INI file.
 */
class IniField {
private:
    std::string value_;

public:
    /**
     * @brief Default constructor.
     */
    IniField() = default;

    /**
     * @brief Construct from a string value.
     * @param value The value to store.
     */
    explicit IniField(std::string value) noexcept : value_(std::move(value)) {}

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
};

}  // namespace inicpp

#endif  // ATOM_EXTRA_INICPP_INIFIELD_HPP

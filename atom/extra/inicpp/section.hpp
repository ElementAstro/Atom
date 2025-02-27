#ifndef ATOM_EXTRA_INICPP_INISECTION_HPP
#define ATOM_EXTRA_INICPP_INISECTION_HPP

#include <map>
#include <optional>
#include <string>

#include "field.hpp"

namespace inicpp {

/**
 * @brief Base class for INI file sections with customizable string comparison.
 * @tparam Comparator The comparator type for field names.
 */
template <typename Comparator>
class IniSectionBase : public std::map<std::string, IniField, Comparator> {
public:
    /**
     * @brief Default constructor.
     */
    IniSectionBase() = default;

    /**
     * @brief Destructor.
     */
    ~IniSectionBase() = default;

    /**
     * @brief Get a field value as the specified type.
     * @tparam T The target type.
     * @param key The field key.
     * @return The converted field value.
     * @throws std::out_of_range if the key doesn't exist.
     * @throws std::invalid_argument if the conversion fails.
     */
    template <typename T>
    [[nodiscard]] T get(const std::string& key) const {
        try {
            auto it = this->find(key);
            if (it == this->end()) {
                throw std::out_of_range("Field '" + key +
                                        "' not found in section");
            }
            return it->second.template as<T>();
        } catch (const std::out_of_range& ex) {
            throw;
        } catch (const std::exception& ex) {
            throw std::invalid_argument("Failed to get field '" + key +
                                        "': " + ex.what());
        }
    }

    /**
     * @brief Get a field value as the specified type with a default value.
     * @tparam T The target type.
     * @param key The field key.
     * @param defaultValue The default value to return if the key doesn't exist
     * or conversion fails.
     * @return The converted field value or the default value.
     */
    template <typename T>
    [[nodiscard]] T get(const std::string& key,
                        const T& defaultValue) const noexcept {
        try {
            auto it = this->find(key);
            if (it == this->end()) {
                return defaultValue;
            }
            return it->second.template as<T>();
        } catch (...) {
            return defaultValue;
        }
    }

    /**
     * @brief Get a field value as the specified type with validation.
     * @tparam T The target type.
     * @param key The field key.
     * @return An optional containing the converted value, or nullopt if the key
     * doesn't exist or conversion fails.
     */
    template <typename T>
    [[nodiscard]] std::optional<T> get_optional(
        const std::string& key) const noexcept {
        try {
            auto it = this->find(key);
            if (it == this->end()) {
                return std::nullopt;
            }
            return it->second.template as_optional<T>();
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Set a field value.
     * @tparam T The type of the value.
     * @param key The field key.
     * @param value The value to set.
     */
    template <typename T>
    void set(const std::string& key, const T& value) {
        try {
            (*this)[key] = value;
        } catch (const std::exception& ex) {
            throw std::invalid_argument("Failed to set field '" + key +
                                        "': " + ex.what());
        }
    }
};

/**
 * @brief Case-sensitive INI section.
 */
using IniSection = IniSectionBase<std::less<std::string>>;

/**
 * @brief Case-insensitive INI section.
 */
using IniSectionCaseInsensitive = IniSectionBase<StringInsensitiveLess>;

}  // namespace inicpp

#endif  // ATOM_EXTRA_INICPP_INISECTION_HPP

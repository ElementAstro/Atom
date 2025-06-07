#pragma once

#include <any>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace modern_log {

/**
 * @class StructuredData
 * @brief Structured log data supporting key-value pairs for rich logging.
 *
 * This class provides a flexible container for structured log fields, allowing
 * arbitrary key-value pairs to be attached to log entries. Values are stored as
 * std::any, enabling heterogeneous types. The class supports adding, removing,
 * querying, merging, and serializing fields, as well as construction from JSON.
 */
class StructuredData {
private:
    std::unordered_map<std::string, std::any>
        fields_;  ///< Internal map of field names to values.

public:
    /**
     * @brief Add a field to the structured data.
     * @tparam T The type of the value.
     * @param key The field name.
     * @param value The field value.
     * @return Reference to this StructuredData for chaining.
     */
    template <typename T>
    StructuredData& add(std::string_view key, T&& value) {
        fields_[std::string(key)] = std::forward<T>(value);
        return *this;
    }

    /**
     * @brief Add multiple fields to the structured data.
     * @tparam Args Variadic argument types (should be key-value pairs).
     * @param args The key-value pairs to add.
     * @return Reference to this StructuredData for chaining.
     *
     * Example usage:
     * @code
     * data.add_fields("user", "alice", "count", 42);
     * @endcode
     */
    template <typename... Args>
    StructuredData& add_fields(Args&&... args) {
        static_assert(sizeof...(Args) % 2 == 0,
                      "Arguments must be key-value pairs");
        auto add_pair = [this](auto&& key, auto&& value) {
            return add(std::forward<decltype(key)>(key),
                       std::forward<decltype(value)>(value));
        };
        (add_pair(std::forward<Args>(args), std::forward<Args>(args + 1)), ...);
        return *this;
    }

    /**
     * @brief Get the value of a field by key.
     * @tparam T The expected type of the value.
     * @param key The field name.
     * @return std::optional containing the value if present and type matches,
     * std::nullopt otherwise.
     */
    template <typename T>
    std::optional<T> get(std::string_view key) const {
        if (auto it = fields_.find(std::string(key)); it != fields_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Check if a field exists in the structured data.
     * @param key The field name.
     * @return True if the field exists, false otherwise.
     */
    bool has(std::string_view key) const;

    /**
     * @brief Remove a field from the structured data.
     * @param key The field name.
     * @return True if the field was removed, false if it did not exist.
     */
    bool remove(std::string_view key);

    /**
     * @brief Clear all fields from the structured data.
     */
    void clear();

    /**
     * @brief Get the number of fields in the structured data.
     * @return The number of fields.
     */
    size_t size() const;

    /**
     * @brief Check if the structured data is empty.
     * @return True if there are no fields, false otherwise.
     */
    bool empty() const;

    /**
     * @brief Serialize the structured data to a JSON string.
     * @return JSON representation of the structured data.
     */
    std::string to_json() const;

    /**
     * @brief Construct structured data from a JSON string.
     * @param json The JSON string.
     * @return StructuredData instance parsed from JSON.
     */
    static StructuredData from_json(std::string_view json);

    /**
     * @brief Merge this structured data with another.
     * @param other The other StructuredData to merge from.
     * @return A new StructuredData containing merged fields (other's fields
     * override on conflict).
     */
    StructuredData merge(const StructuredData& other) const;

    /**
     * @brief Get a vector of all field keys.
     * @return Vector of field names.
     */
    std::vector<std::string> keys() const;

private:
    /**
     * @brief Convert a std::any value to a string for serialization.
     * @param value The value to convert.
     * @return String representation of the value.
     */
    std::string any_to_string(const std::any& value) const;
};

}  // namespace modern_log
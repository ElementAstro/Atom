#ifndef ATOM_SECRET_SERIALIZATION_JSON_HPP
#define ATOM_SECRET_SERIALIZATION_JSON_HPP

#include <chrono>
#include <string>
#include <vector>

#include "atom/type/json.hpp"

#include "../core/result.hpp"
#include "../password/entry.hpp"

namespace atom::secret {

// Use nlohmann::json from atom/type/json.hpp
using json = nlohmann::json;

/**
 * @brief JSON serialization and deserialization utilities.
 *
 * Uses nlohmann::json library from atom/type/json.hpp for robust JSON handling.
 */
class JsonSerializer {
public:
    /**
     * @brief Serializes a PasswordEntry to JSON string.
     * @param entry The password entry to serialize.
     * @param pretty Whether to format with indentation (default: false).
     * @return Result containing JSON string or error.
     */
    static Result<std::string> serializeEntry(const PasswordEntry& entry,
                                              bool pretty = false);

    /**
     * @brief Deserializes a PasswordEntry from JSON string.
     * @param jsonStr The JSON string to deserialize.
     * @return Result containing PasswordEntry or error.
     */
    static Result<PasswordEntry> deserializeEntry(const std::string& jsonStr);

    /**
     * @brief Serializes multiple PasswordEntry objects to JSON array.
     * @param entries Vector of password entries to serialize.
     * @param pretty Whether to format with indentation (default: false).
     * @return Result containing JSON string or error.
     */
    static Result<std::string> serializeEntries(
        const std::vector<PasswordEntry>& entries, bool pretty = false);

    /**
     * @brief Deserializes multiple PasswordEntry objects from JSON array.
     * @param jsonStr The JSON array string to deserialize.
     * @return Result containing vector of PasswordEntry or error.
     */
    static Result<std::vector<PasswordEntry>> deserializeEntries(
        const std::string& jsonStr);

    /**
     * @brief Serializes entries with their keys to JSON object.
     * @param entries Map of key to entry.
     * @param pretty Whether to format with indentation (default: false).
     * @return Result containing JSON string or error.
     */
    static Result<std::string> serializeEntriesWithKeys(
        const std::vector<std::pair<std::string, PasswordEntry>>& entries,
        bool pretty = false);

    /**
     * @brief Deserializes entries with their keys from JSON object.
     * @param jsonStr The JSON string.
     * @return Result containing vector of key-entry pairs or error.
     */
    static Result<std::vector<std::pair<std::string, PasswordEntry>>>
    deserializeEntriesWithKeys(const std::string& jsonStr);

    /**
     * @brief Converts a PasswordEntry to nlohmann::json object.
     * @param entry The password entry to convert.
     * @return JSON object.
     */
    static json entryToJson(const PasswordEntry& entry);

    /**
     * @brief Converts a nlohmann::json object to PasswordEntry.
     * @param j The JSON object.
     * @return PasswordEntry.
     */
    static PasswordEntry jsonToEntry(const json& j);

    /**
     * @brief Converts time_point to ISO 8601 string.
     * @param timePoint Time point to convert.
     * @return ISO 8601 formatted string.
     */
    static std::string timePointToString(
        const std::chrono::system_clock::time_point& timePoint);

    /**
     * @brief Converts ISO 8601 string to time_point.
     * @param timeString ISO 8601 formatted string.
     * @return Time point.
     */
    static std::chrono::system_clock::time_point stringToTimePoint(
        const std::string& timeString);
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_SERIALIZATION_JSON_HPP

#ifndef ATOM_SECRET_SERIALIZATION_HPP
#define ATOM_SECRET_SERIALIZATION_HPP

#include <chrono>
#include <string>
#include <vector>

#include "password_entry.hpp"
#include "result.hpp"

namespace atom::secret {

/**
 * @brief JSON serialization and deserialization utilities for password entries.
 */
class JsonSerializer {
public:
    /**
     * @brief Serializes a PasswordEntry to JSON string.
     * @param entry The password entry to serialize.
     * @return Result containing JSON string or error message.
     */
    static Result<std::string> serializeEntry(const PasswordEntry& entry);

    /**
     * @brief Deserializes a PasswordEntry from JSON string.
     * @param json The JSON string to deserialize.
     * @return Result containing PasswordEntry or error message.
     */
    static Result<PasswordEntry> deserializeEntry(const std::string& json);

    /**
     * @brief Serializes multiple PasswordEntry objects to JSON array.
     * @param entries Vector of password entries to serialize.
     * @return Result containing JSON string or error message.
     */
    static Result<std::string> serializeEntries(
        const std::vector<PasswordEntry>& entries);

    /**
     * @brief Deserializes multiple PasswordEntry objects from JSON array.
     * @param json The JSON array string to deserialize.
     * @return Result containing vector of PasswordEntry or error message.
     */
    static Result<std::vector<PasswordEntry>> deserializeEntries(
        const std::string& json);

    /**
     * @brief Serializes a PasswordManagerSettings object to JSON.
     * @param settings The settings to serialize.
     * @return Result containing JSON string or error message.
     */
    static Result<std::string> serializeSettings(
        const PasswordManagerSettings& settings);

    /**
     * @brief Deserializes a PasswordManagerSettings object from JSON.
     * @param json The JSON string to deserialize.
     * @return Result containing PasswordManagerSettings or error message.
     */
    static Result<PasswordManagerSettings> deserializeSettings(
        const std::string& json);

    /**
     * @brief Unescapes a JSON string (public utility).
     * @param str Escaped JSON string.
     * @return Unescaped string.
     */
    static std::string unescapeString(const std::string& str);

private:
    /**
     * @brief Escapes a string for JSON format.
     * @param str String to escape.
     * @return Escaped string.
     */
    static std::string escapeJsonString(const std::string& str);

    /**
     * @brief Unescapes a JSON string.
     * @param str Escaped JSON string.
     * @return Unescaped string.
     */
    static std::string unescapeJsonString(const std::string& str);

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

    /**
     * @brief Converts PasswordCategory enum to string.
     * @param category Category to convert.
     * @return String representation.
     */
    static std::string categoryToString(PasswordCategory category);

    /**
     * @brief Converts string to PasswordCategory enum.
     * @param str String to convert.
     * @return PasswordCategory enum value.
     */
    static PasswordCategory stringToCategory(const std::string& str);

    /**
     * @brief Converts EncryptionOptions::Method enum to string.
     * @param method Method to convert.
     * @return String representation.
     */
    static std::string methodToString(EncryptionOptions::Method method);

    /**
     * @brief Converts string to EncryptionOptions::Method enum.
     * @param str String to convert.
     * @return EncryptionOptions::Method enum value.
     */
    static EncryptionOptions::Method stringToMethod(const std::string& str);

    /**
     * @brief Serializes a vector of strings to JSON array.
     * @param strings Vector of strings to serialize.
     * @return JSON array string.
     */
    static std::string serializeStringArray(
        const std::vector<std::string>& strings);

    /**
     * @brief Deserializes a JSON array to vector of strings.
     * @param json JSON array string.
     * @return Vector of strings.
     */
    static std::vector<std::string> deserializeStringArray(
        const std::string& json);
};

/**
 * @brief Simple JSON parser for basic JSON operations.
 * This is a minimal implementation to avoid external dependencies.
 */
class SimpleJsonParser {
public:
    /**
     * @brief Extracts a string value from JSON.
     * @param json JSON string.
     * @param key Key to extract.
     * @return Extracted string value or empty string if not found.
     */
    static std::string extractString(const std::string& json,
                                     const std::string& key);

    /**
     * @brief Extracts an integer value from JSON.
     * @param json JSON string.
     * @param key Key to extract.
     * @return Extracted integer value or 0 if not found.
     */
    static int extractInt(const std::string& json, const std::string& key);

    /**
     * @brief Extracts a boolean value from JSON.
     * @param json JSON string.
     * @param key Key to extract.
     * @return Extracted boolean value or false if not found.
     */
    static bool extractBool(const std::string& json, const std::string& key);

    /**
     * @brief Extracts an array value from JSON.
     * @param json JSON string.
     * @param key Key to extract.
     * @return Extracted array as string or empty string if not found.
     */
    static std::string extractArray(const std::string& json,
                                    const std::string& key);

    /**
     * @brief Extracts an object value from JSON.
     * @param json JSON string.
     * @param key Key to extract.
     * @return Extracted object as string or empty string if not found.
     */
    static std::string extractObject(const std::string& json,
                                     const std::string& key);

    /**
     * @brief Checks if JSON string is valid.
     * @param json JSON string to validate.
     * @return True if valid, false otherwise.
     */
    static bool isValidJson(const std::string& json);

private:
    /**
     * @brief Finds the value for a given key in JSON.
     * @param json JSON string.
     * @param key Key to find.
     * @param startPos Starting position for search.
     * @param endPos Ending position of the value.
     * @return Starting position of the value or std::string::npos if not found.
     */
    static size_t findValue(const std::string& json, const std::string& key,
                            size_t startPos, size_t& endPos);

    /**
     * @brief Skips whitespace characters.
     * @param json JSON string.
     * @param pos Current position.
     * @return Position after whitespace.
     */
    static size_t skipWhitespace(const std::string& json, size_t pos);

    /**
     * @brief Finds the end of a JSON value.
     * @param json JSON string.
     * @param startPos Starting position of the value.
     * @return Position after the value.
     */
    static size_t findValueEnd(const std::string& json, size_t startPos);
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_SERIALIZATION_HPP

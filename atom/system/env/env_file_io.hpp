/*
 * env_file_io.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Environment variable file I/O operations

**************************************************/

#ifndef ATOM_SYSTEM_ENV_FILE_IO_HPP
#define ATOM_SYSTEM_ENV_FILE_IO_HPP

#include <filesystem>

#include "atom/containers/high_performance.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;

/**
 * @brief Environment variable file I/O operations
 */
class EnvFileIO {
public:
    /**
     * @brief Saves environment variables to a file
     * @param filePath The path to the file
     * @param vars The map of variables to save, or all environment variables if empty
     * @return True if the save was successful, otherwise false
     */
    static auto saveToFile(const std::filesystem::path& filePath,
                           const HashMap<String, String>& vars = {}) -> bool;

    /**
     * @brief Loads environment variables from a file
     * @param filePath The path to the file
     * @param overwrite Whether to overwrite existing variables
     * @return True if the load was successful, otherwise false
     */
    static auto loadFromFile(const std::filesystem::path& filePath,
                             bool overwrite = false) -> bool;

private:
    /**
     * @brief Parses a line from an environment file
     * @param line The line to parse
     * @return A pair of key and value, or empty strings if parsing failed
     */
    static auto parseLine(const String& line) -> std::pair<String, String>;

    /**
     * @brief Formats a key-value pair for writing to file
     * @param key The environment variable key
     * @param value The environment variable value
     * @return Formatted string for writing to file
     */
    static auto formatLine(const String& key, const String& value) -> String;

    /**
     * @brief Validates an environment variable key
     * @param key The key to validate
     * @return True if the key is valid, otherwise false
     */
    static auto isValidKey(const String& key) -> bool;

    /**
     * @brief Escapes special characters in a value
     * @param value The value to escape
     * @return Escaped value
     */
    static auto escapeValue(const String& value) -> String;

    /**
     * @brief Unescapes special characters in a value
     * @param value The value to unescape
     * @return Unescaped value
     */
    static auto unescapeValue(const String& value) -> String;
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_FILE_IO_HPP

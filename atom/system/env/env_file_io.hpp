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
 * @brief File format enumeration for environment files
 */
enum class EnvFileFormat {
    AUTO,       // Auto-detect format
    DOTENV,     // .env format (KEY=value)
    SHELL,      // Shell script format (export KEY=value)
    JSON,       // JSON format
    YAML,       // YAML format
    XML,        // XML format
    INI         // INI format
};

/**
 * @brief Environment variable file I/O operations
 */
class EnvFileIO {
public:
    /**
     * @brief Saves environment variables to a file
     * @param filePath The path to the file
     * @param vars The map of variables to save, or all environment variables if empty
     * @param format File format to use
     * @return True if the save was successful, otherwise false
     */
    static auto saveToFile(const std::filesystem::path& filePath,
                           const HashMap<String, String>& vars = {},
                           EnvFileFormat format = EnvFileFormat::AUTO) -> bool;

    /**
     * @brief Loads environment variables from a file
     * @param filePath The path to the file
     * @param overwrite Whether to overwrite existing variables
     * @param format File format to use (AUTO for auto-detection)
     * @return True if the load was successful, otherwise false
     */
    static auto loadFromFile(const std::filesystem::path& filePath,
                             bool overwrite = false,
                             EnvFileFormat format = EnvFileFormat::AUTO) -> bool;

    /**
     * @brief Saves environment variables to a file atomically
     * @param filePath The path to the file
     * @param vars The map of variables to save
     * @param format File format to use
     * @return True if the save was successful, otherwise false
     */
    static auto saveToFileAtomic(const std::filesystem::path& filePath,
                                 const HashMap<String, String>& vars,
                                 EnvFileFormat format = EnvFileFormat::AUTO) -> bool;

    /**
     * @brief Validates an environment file without loading it
     * @param filePath The path to the file
     * @param format File format to use (AUTO for auto-detection)
     * @return True if the file is valid, otherwise false
     */
    static auto validateFile(const std::filesystem::path& filePath,
                             EnvFileFormat format = EnvFileFormat::AUTO) -> bool;

    /**
     * @brief Gets file format from file extension or content
     * @param filePath The path to the file
     * @return Detected file format
     */
    static auto detectFileFormat(const std::filesystem::path& filePath) -> EnvFileFormat;

    /**
     * @brief Merges multiple environment files
     * @param filePaths Vector of file paths to merge
     * @param outputPath Output file path
     * @param format Output file format
     * @return True if merge was successful, otherwise false
     */
    static auto mergeFiles(const std::vector<std::filesystem::path>& filePaths,
                           const std::filesystem::path& outputPath,
                           EnvFileFormat format = EnvFileFormat::DOTENV) -> bool;

private:
    /**
     * @brief Parses a line from an environment file
     * @param line The line to parse
     * @param format File format being parsed
     * @return A pair of key and value, or empty strings if parsing failed
     */
    static auto parseLine(const String& line, EnvFileFormat format = EnvFileFormat::DOTENV)
        -> std::pair<String, String>;

    /**
     * @brief Formats a key-value pair for writing to file
     * @param key The environment variable key
     * @param value The environment variable value
     * @param format File format to use
     * @return Formatted string for writing to file
     */
    static auto formatLine(const String& key, const String& value,
                           EnvFileFormat format = EnvFileFormat::DOTENV) -> String;

    /**
     * @brief Validates an environment variable key
     * @param key The key to validate
     * @return True if the key is valid, otherwise false
     */
    static auto isValidKey(const String& key) -> bool;

    /**
     * @brief Escapes special characters in a value
     * @param value The value to escape
     * @param format File format to use for escaping
     * @return Escaped value
     */
    static auto escapeValue(const String& value, EnvFileFormat format = EnvFileFormat::DOTENV) -> String;

    /**
     * @brief Unescapes special characters in a value
     * @param value The value to unescape
     * @param format File format to use for unescaping
     * @return Unescaped value
     */
    static auto unescapeValue(const String& value, EnvFileFormat format = EnvFileFormat::DOTENV) -> String;

    // Format-specific parsers
    static auto parseJsonFile(const std::filesystem::path& filePath) -> HashMap<String, String>;
    static auto parseYamlFile(const std::filesystem::path& filePath) -> HashMap<String, String>;
    static auto parseXmlFile(const std::filesystem::path& filePath) -> HashMap<String, String>;
    static auto parseIniFile(const std::filesystem::path& filePath) -> HashMap<String, String>;
    static auto parseShellFile(const std::filesystem::path& filePath) -> HashMap<String, String>;

    // Format-specific writers
    static auto writeJsonFile(const std::filesystem::path& filePath,
                              const HashMap<String, String>& vars) -> bool;
    static auto writeYamlFile(const std::filesystem::path& filePath,
                              const HashMap<String, String>& vars) -> bool;
    static auto writeXmlFile(const std::filesystem::path& filePath,
                             const HashMap<String, String>& vars) -> bool;
    static auto writeIniFile(const std::filesystem::path& filePath,
                             const HashMap<String, String>& vars) -> bool;
    static auto writeShellFile(const std::filesystem::path& filePath,
                               const HashMap<String, String>& vars) -> bool;

    // Utility methods
    static auto createBackupFile(const std::filesystem::path& filePath) -> bool;
    static auto trimString(const String& str) -> String;
    static auto isCommentLine(const String& line) -> bool;
    static auto getFileExtension(const std::filesystem::path& filePath) -> String;
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_FILE_IO_HPP

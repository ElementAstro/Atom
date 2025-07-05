#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace dotenv {

/**
 * @brief Configuration options for loading .env files.
 *
 * This struct defines various options that control how .env files are loaded,
 * including whether to override existing environment variables, whether to
 * create missing files, encoding settings, search paths, and file patterns.
 */
struct LoadOptions {
    /**
     * @brief If true, override existing environment variables with loaded
     * values.
     */
    bool override_existing = false;

    /**
     * @brief If true, create the .env file if it does not exist.
     */
    bool create_if_missing = false;

    /**
     * @brief The expected encoding of the .env file (e.g., "utf-8").
     */
    std::string encoding = "utf-8";

    /**
     * @brief List of directories to search for .env files.
     */
    std::vector<std::string> search_paths = {".", "./config", "../config"};

    /**
     * @brief List of file name patterns to match when searching for .env files.
     * Wildcards '*' and '?' are supported.
     */
    std::vector<std::string> file_patterns = {".env", ".env.local",
                                              ".env.development"};
};

/**
 * @brief Cross-platform file loader for .env files with advanced features.
 *
 * This class provides methods to load, save, and discover .env files,
 * supporting encoding detection, file pattern matching, and file accessibility
 * checks.
 */
class FileLoader {
public:
    /**
     * @brief Construct a FileLoader with the given options.
     * @param options Configuration options for loading files.
     */
    explicit FileLoader(const LoadOptions& options = LoadOptions{});

    /**
     * @brief Load the content of a .env file from the specified path.
     * @param filepath Path to the .env file.
     * @return The content of the file as a string.
     * @throws std::filesystem::filesystem_error if the file does not exist or
     * is not accessible.
     */
    std::string load(const std::filesystem::path& filepath);

    /**
     * @brief Load and combine content from multiple .env files.
     * @param filepaths Vector of file paths to load.
     * @return Combined content of all files as a single string.
     */
    std::string loadMultiple(
        const std::vector<std::filesystem::path>& filepaths);

    /**
     * @brief Automatically discover and load .env files from search paths.
     * @param base_path Base directory to start searching from.
     * @return Combined content from all discovered files.
     */
    std::string autoLoad(const std::filesystem::path& base_path = ".");

    /**
     * @brief Save environment variables to a .env file.
     * @param filepath Output file path.
     * @param env_vars Environment variables to save as key-value pairs.
     * @throws std::filesystem::filesystem_error if the file cannot be written.
     */
    void save(const std::filesystem::path& filepath,
              const std::unordered_map<std::string, std::string>& env_vars);

    /**
     * @brief Check if a file exists and is readable.
     * @param filepath Path to the file.
     * @return True if the file exists and is readable, false otherwise.
     */
    bool isAccessible(const std::filesystem::path& filepath);

    /**
     * @brief Get the last modification time of a file.
     * @param filepath Path to the file.
     * @return The file's last modification time.
     * @throws std::filesystem::filesystem_error if the file does not exist.
     */
    std::filesystem::file_time_type getModificationTime(
        const std::filesystem::path& filepath);

private:
    /**
     * @brief Loader configuration options.
     */
    LoadOptions options_;

    /**
     * @brief Read the content of a file as a string.
     * @param filepath Path to the file.
     * @return File content as a string.
     * @throws std::filesystem::filesystem_error if the file cannot be read.
     */
    std::string readFile(const std::filesystem::path& filepath);

    /**
     * @brief Discover .env files in the specified base path using search paths
     * and patterns.
     * @param base_path Base directory to search from.
     * @return Vector of discovered file paths.
     */
    std::vector<std::filesystem::path> discoverFiles(
        const std::filesystem::path& base_path);

    /**
     * @brief Check if a filename matches a given pattern (supports '*' and
     * '?').
     * @param filename Name of the file.
     * @param pattern Pattern to match against.
     * @return True if the filename matches the pattern, false otherwise.
     */
    bool matchesPattern(const std::string& filename,
                        const std::string& pattern);

    /**
     * @brief Detect the encoding of the file content.
     * @param content File content as a string.
     * @return Detected encoding as a string (e.g., "utf-8").
     */
    std::string detectEncoding(const std::string& content);

    /**
     * @brief Convert file content from one encoding to another.
     * @param content File content as a string.
     * @param from_encoding The encoding to convert from.
     * @return Converted content as a string.
     */
    std::string convertEncoding(const std::string& content,
                                const std::string& from_encoding);
};

}  // namespace dotenv

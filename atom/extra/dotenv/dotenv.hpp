#pragma once

#include "loader.hpp"
#include "parser.hpp"
#include "validator.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

namespace dotenv {

/**
 * @brief Configuration options for the Dotenv loader.
 *
 * This struct encapsulates all configuration options for the Dotenv loader,
 * including parser options, loader options, debug mode, and a custom logger.
 */
struct DotenvOptions {
    /**
     * @brief Options for parsing .env files.
     */
    ParseOptions parse_options;

    /**
     * @brief Options for loading .env files from disk.
     */
    LoadOptions load_options;

    /**
     * @brief Enable debug logging if true.
     */
    bool debug = false;

    /**
     * @brief Optional logger callback for debug or error messages.
     */
    std::function<void(const std::string&)> logger = nullptr;
};

/**
 * @brief Result of loading environment variables from .env files.
 *
 * This struct contains the outcome of a load operation, including the loaded
 * variables, any errors or warnings encountered, and the list of files loaded.
 */
struct LoadResult {
    /**
     * @brief True if loading was successful, false otherwise.
     */
    bool success = true;

    /**
     * @brief Map of loaded environment variables (key-value pairs).
     */
    std::unordered_map<std::string, std::string> variables;

    /**
     * @brief List of error messages encountered during loading.
     */
    std::vector<std::string> errors;

    /**
     * @brief List of warning messages encountered during loading.
     */
    std::vector<std::string> warnings;

    /**
     * @brief List of file paths that were loaded.
     */
    std::vector<std::filesystem::path> loaded_files;

    /**
     * @brief Add an error message and mark the result as unsuccessful.
     * @param error Error message to add.
     */
    void addError(const std::string& error) {
        errors.push_back(error);
        success = false;
    }

    /**
     * @brief Add a warning message.
     * @param warning Warning message to add.
     */
    void addWarning(const std::string& warning) { warnings.push_back(warning); }
};

/**
 * @brief Main Dotenv class for loading and managing environment variables.
 *
 * This class provides a modern C++ interface for loading, parsing, validating,
 * and applying environment variables from .env files. It supports advanced
 * features such as schema validation, file watching, and custom logging.
 */
class Dotenv {
public:
    /**
     * @brief Construct a Dotenv loader with the specified options.
     * @param options Configuration options for the loader.
     */
    explicit Dotenv(const DotenvOptions& options = DotenvOptions{});

    /**
     * @brief Load environment variables from a single .env file.
     * @param filepath Path to the .env file (default: ".env").
     * @return LoadResult containing loaded variables and status.
     */
    LoadResult load(const std::filesystem::path& filepath = ".env");

    /**
     * @brief Load environment variables from multiple .env files.
     * @param filepaths Vector of file paths to load.
     * @return LoadResult containing combined variables and status.
     */
    LoadResult loadMultiple(
        const std::vector<std::filesystem::path>& filepaths);

    /**
     * @brief Automatically discover and load .env files from search paths.
     * @param base_path Base directory for file discovery (default: ".").
     * @return LoadResult containing discovered variables and status.
     */
    LoadResult autoLoad(const std::filesystem::path& base_path = ".");

    /**
     * @brief Load environment variables from a string containing .env content.
     * @param content The .env file content as a string.
     * @return LoadResult containing parsed variables and status.
     */
    LoadResult loadFromString(const std::string& content);

    /**
     * @brief Load and validate environment variables using a schema.
     * @param filepath Path to the .env file.
     * @param schema Validation schema to apply.
     * @return LoadResult containing validation results and variables.
     */
    LoadResult loadAndValidate(const std::filesystem::path& filepath,
                               const ValidationSchema& schema);

    /**
     * @brief Apply loaded variables to the system environment.
     * @param variables Map of variables to apply.
     * @param override_existing If true, override existing environment
     * variables.
     */
    void applyToEnvironment(
        const std::unordered_map<std::string, std::string>& variables,
        bool override_existing = false);

    /**
     * @brief Save environment variables to a .env file.
     * @param filepath Output file path.
     * @param variables Map of variables to save.
     */
    void save(const std::filesystem::path& filepath,
              const std::unordered_map<std::string, std::string>& variables);

    /**
     * @brief Watch a .env file for changes and reload automatically.
     * @param filepath File to watch for changes.
     * @param callback Callback function invoked when the file changes.
     */
    void watch(const std::filesystem::path& filepath,
               std::function<void(const LoadResult&)> callback);

    /**
     * @brief Stop watching the file for changes.
     */
    void stopWatching();

    /**
     * @brief Get the current configuration options.
     * @return Reference to the current DotenvOptions.
     */
    const DotenvOptions& getOptions() const { return options_; }

    /**
     * @brief Update the configuration options.
     * @param options New configuration options to set.
     */
    void setOptions(const DotenvOptions& options) { options_ = options; }

    // Static convenience methods

    /**
     * @brief Quickly load environment variables from a file with default
     * options.
     * @param filepath Path to the .env file (default: ".env").
     * @return LoadResult containing loaded variables and status.
     */
    static LoadResult quickLoad(const std::filesystem::path& filepath = ".env");

    /**
     * @brief Quickly load and apply environment variables to the system
     * environment.
     * @param filepath Path to the .env file (default: ".env").
     * @param override_existing If true, override existing environment
     * variables.
     */
    static void config(const std::filesystem::path& filepath = ".env",
                       bool override_existing = false);

private:
    /**
     * @brief Current configuration options.
     */
    DotenvOptions options_;

    /**
     * @brief Parser instance for .env files.
     */
    std::unique_ptr<Parser> parser_;

    /**
     * @brief Validator instance for schema validation.
     */
    std::unique_ptr<Validator> validator_;

    /**
     * @brief File loader instance for reading and writing files.
     */
    std::unique_ptr<FileLoader> loader_;

    /**
     * @brief Thread for file watching.
     */
    std::unique_ptr<std::thread> watcher_thread_;

    /**
     * @brief Atomic flag indicating whether file watching is active.
     */
    std::atomic<bool> watching_{false};

    /**
     * @brief Log a message using the configured logger or standard output.
     * @param message Message to log.
     */
    void log(const std::string& message);

    /**
     * @brief Initialize internal components (parser, loader, validator).
     */
    void initializeComponents();

    /**
     * @brief Process loaded .env content and return a LoadResult.
     * @param content The loaded .env content as a string.
     * @param source_files Optional vector of source file paths.
     * @return LoadResult containing variables and status.
     */
    LoadResult processLoadedContent(
        const std::string& content,
        const std::vector<std::filesystem::path>& source_files = {});
};

}  // namespace dotenv
/*
 * env.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Environment variable management

**************************************************/

#ifndef ATOM_UTILS_ENV_HPP
#define ATOM_UTILS_ENV_HPP

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <tuple>
#include <type_traits>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

/**
 * @brief Environment variable format enumeration
 */
enum class VariableFormat {
    UNIX,     // ${VAR} or $VAR format
    WINDOWS,  // %VAR% format
    AUTO      // Auto-detect based on platform
};

/**
 * @brief Environment variable persistence level enumeration
 */
enum class PersistLevel {
    PROCESS,  // Current process only
    USER,     // User level persistence
    SYSTEM    // System level persistence (requires admin privileges)
};

/**
 * @brief Environment variable class for managing program environment variables,
 * command-line arguments, and other related information.
 */
class Env {
public:
    /**
     * @brief Default constructor that initializes environment variable
     * information.
     */
    Env();

    /**
     * @brief Constructor that initializes environment variable information with
     * command-line arguments.
     * @param argc Number of command-line arguments.
     * @param argv Array of command-line arguments.
     */
    explicit Env(int argc, char** argv);

    /**
     * @brief Static method to create a shared pointer to an Env object.
     * @param argc Number of command-line arguments.
     * @param argv Array of command-line arguments.
     * @return Shared pointer to an Env object.
     */
    static auto createShared(int argc, char** argv) -> std::shared_ptr<Env>;

    /**
     * @brief Static method to get the current environment variables.
     * @return HashMap of environment variables.
     */
    static auto Environ() -> HashMap<String, String>;

    /**
     * @brief Adds a key-value pair to the environment variables.
     * @param key The key name.
     * @param val The value associated with the key.
     */
    void add(const String& key, const String& val);

    /**
     * @brief Adds multiple key-value pairs to the environment variables.
     * @param vars The map of key-value pairs to add.
     */
    void addMultiple(const HashMap<String, String>& vars);

    /**
     * @brief Checks if a key exists in the environment variables.
     * @param key The key name.
     * @return True if the key exists, otherwise false.
     */
    bool has(const String& key);

    /**
     * @brief Checks if all keys exist in the environment variables.
     * @param keys The vector of key names.
     * @return True if all keys exist, otherwise false.
     */
    bool hasAll(const Vector<String>& keys);

    /**
     * @brief Checks if any of the keys exist in the environment variables.
     * @param keys The vector of key names.
     * @return True if any key exists, otherwise false.
     */
    bool hasAny(const Vector<String>& keys);

    /**
     * @brief Deletes a key-value pair from the environment variables.
     * @param key The key name.
     */
    void del(const String& key);

    /**
     * @brief Deletes multiple key-value pairs from the environment variables.
     * @param keys The vector of key names to delete.
     */
    void delMultiple(const Vector<String>& keys);

    /**
     * @brief Gets the value associated with a key, or returns a default value
     * if the key does not exist.
     * @param key The key name.
     * @param default_value The default value to return if the key does not
     * exist.
     * @return The value associated with the key, or the default value.
     */
    ATOM_NODISCARD auto get(const String& key, const String& default_value = "")
        -> String;

    /**
     * @brief Gets the value associated with a key and converts it to the
     * specified type.
     * @tparam T The type to convert the value to.
     * @param key The key name.
     * @param default_value The default value to return if the key does not
     * exist or conversion fails.
     * @return The value converted to type T, or the default value.
     */
    template <typename T>
    ATOM_NODISCARD auto getAs(const String& key, const T& default_value = T())
        -> T;

    /**
     * @brief Gets the value associated with a key as an optional type.
     * @tparam T The type to convert the value to.
     * @param key The key name.
     * @return An optional containing the value if it exists and can be
     * converted, otherwise empty.
     */
    template <typename T>
    ATOM_NODISCARD auto getOptional(const String& key) -> std::optional<T>;

    /**
     * @brief Sets the value of an environment variable.
     * @param key The key name.
     * @param val The value to set.
     * @return True if the environment variable was set successfully, otherwise
     * false.
     */
    static auto setEnv(const String& key, const String& val) -> bool;

    /**
     * @brief Sets multiple environment variables.
     * @param vars The map of key-value pairs to set.
     * @return True if all environment variables were set successfully,
     * otherwise false.
     */
    static auto setEnvMultiple(const HashMap<String, String>& vars) -> bool;

    /**
     * @brief Gets the value of an environment variable, or returns a default
     * value if the variable does not exist.
     * @param key The key name.
     * @param default_value The default value to return if the variable does not
     * exist.
     * @return The value of the environment variable, or the default value.
     */
    ATOM_NODISCARD static auto getEnv(const String& key,
                                      const String& default_value = "")
        -> String;

    /**
     * @brief Gets the value of an environment variable and converts it to the
     * specified type.
     * @tparam T The type to convert the value to.
     * @param key The key name.
     * @param default_value The default value to return if the variable does not
     * exist or conversion fails.
     * @return The value converted to type T, or the default value.
     */
    template <typename T>
    ATOM_NODISCARD static auto getEnvAs(const String& key,
                                        const T& default_value = T()) -> T;

    /**
     * @brief Unsets an environment variable.
     * @param name The name of the environment variable to unset.
     */
    static void unsetEnv(const String& name);

    /**
     * @brief Unsets multiple environment variables.
     * @param names The vector of environment variable names to unset.
     */
    static void unsetEnvMultiple(const Vector<String>& names);

    /**
     * @brief Lists all environment variables.
     * @return A vector of environment variable names.
     */
    static auto listVariables() -> Vector<String>;

    /**
     * @brief Filters environment variables based on a predicate.
     * @param predicate The predicate function that takes a key-value pair and
     * returns a boolean.
     * @return A map of filtered environment variables.
     */
    static auto filterVariables(
        const std::function<bool(const String&, const String&)>& predicate)
        -> HashMap<String, String>;

    /**
     * @brief Gets all environment variables that start with a given prefix.
     * @param prefix The prefix to filter by.
     * @return A map of environment variables with the given prefix.
     */
    static auto getVariablesWithPrefix(const String& prefix)
        -> HashMap<String, String>;

    /**
     * @brief Saves environment variables to a file.
     * @param filePath The path to the file.
     * @param vars The map of variables to save, or all environment variables if
     * empty.
     * @return True if the save was successful, otherwise false.
     */
    static auto saveToFile(const std::filesystem::path& filePath,
                           const HashMap<String, String>& vars = {}) -> bool;

    /**
     * @brief Loads environment variables from a file.
     * @param filePath The path to the file.
     * @param overwrite Whether to overwrite existing variables.
     * @return True if the load was successful, otherwise false.
     */
    static auto loadFromFile(const std::filesystem::path& filePath,
                             bool overwrite = false) -> bool;

    /**
     * @brief Gets the executable path.
     * @return The full path of the executable file.
     */
    ATOM_NODISCARD auto getExecutablePath() const -> String;

    /**
     * @brief Gets the working directory.
     * @return The working directory.
     */
    ATOM_NODISCARD auto getWorkingDirectory() const -> String;

    /**
     * @brief Gets the program name.
     * @return The program name.
     */
    ATOM_NODISCARD auto getProgramName() const -> String;

    /**
     * @brief Gets all command-line arguments.
     * @return The map of command-line arguments.
     */
    ATOM_NODISCARD auto getAllArgs() const -> HashMap<String, String>;

    /**
     * @brief Gets the user home directory.
     * @return The path to the user home directory.
     */
    ATOM_NODISCARD static auto getHomeDir() -> String;

    /**
     * @brief Gets the system temporary directory.
     * @return The path to the system temporary directory.
     */
    ATOM_NODISCARD static auto getTempDir() -> String;

    /**
     * @brief Gets the system configuration directory.
     * @return The path to the system configuration directory.
     */
    ATOM_NODISCARD static auto getConfigDir() -> String;

    /**
     * @brief Gets the user data directory.
     * @return The path to the user data directory.
     */
    ATOM_NODISCARD static auto getDataDir() -> String;

    /**
     * @brief Expands environment variable references in a string.
     * @param str String containing environment variable references (e.g.,
     * "$HOME/file" or "%PATH%;newpath")
     * @param format Environment variable format, can be Unix style (${VAR}) or
     * Windows style (%VAR%)
     * @return Expanded string.
     */
    ATOM_NODISCARD static auto expandVariables(
        const String& str, VariableFormat format = VariableFormat::AUTO)
        -> String;

    /**
     * @brief Sets a persistent environment variable.
     * @param key Environment variable name.
     * @param val Environment variable value.
     * @param level Persistence level.
     * @return True if successfully persisted, otherwise false.
     */
    static auto setPersistentEnv(const String& key, const String& val,
                                 PersistLevel level = PersistLevel::USER)
        -> bool;

    /**
     * @brief Deletes a persistent environment variable.
     * @param key Environment variable name.
     * @param level Persistence level.
     * @return True if successfully deleted, otherwise false.
     */
    static auto deletePersistentEnv(const String& key,
                                    PersistLevel level = PersistLevel::USER)
        -> bool;

    /**
     * @brief Adds a path to the PATH environment variable.
     * @param path Path to add.
     * @param prepend Whether to add to the beginning (default adds to end).
     * @return True if successfully added, otherwise false.
     */
    static auto addToPath(const String& path, bool prepend = false) -> bool;

    /**
     * @brief Removes a path from the PATH environment variable.
     * @param path Path to remove.
     * @return True if successfully removed, otherwise false.
     */
    static auto removeFromPath(const String& path) -> bool;

    /**
     * @brief Checks if a path is in the PATH environment variable.
     * @param path Path to check.
     * @return True if in PATH, otherwise false.
     */
    ATOM_NODISCARD static auto isInPath(const String& path) -> bool;

    /**
     * @brief Gets all paths in the PATH environment variable.
     * @return Vector containing all paths.
     */
    ATOM_NODISCARD static auto getPathEntries() -> Vector<String>;

    /**
     * @brief Compares differences between two environment variable sets.
     * @param env1 First environment variable set.
     * @param env2 Second environment variable set.
     * @return Difference content, including added, removed, and modified
     * variables.
     */
    ATOM_NODISCARD static auto diffEnvironments(
        const HashMap<String, String>& env1,
        const HashMap<String, String>& env2)
        -> std::tuple<HashMap<String, String>,   // Added variables
                      HashMap<String, String>,   // Removed variables
                      HashMap<String, String>>;  // Modified variables

    /**
     * @brief Merges two environment variable sets.
     * @param baseEnv Base environment variable set.
     * @param overlayEnv Overlay environment variable set.
     * @param override Whether to override base environment variables in case of
     * conflict.
     * @return Merged environment variable set.
     */
    ATOM_NODISCARD static auto mergeEnvironments(
        const HashMap<String, String>& baseEnv,
        const HashMap<String, String>& overlayEnv, bool override = true)
        -> HashMap<String, String>;

    /**
     * @brief Gets the system name.
     * @return System name (e.g., "Windows", "Linux", "macOS").
     */
    ATOM_NODISCARD static auto getSystemName() -> String;

    /**
     * @brief Gets the system architecture.
     * @return System architecture (e.g., "x86_64", "arm64").
     */
    ATOM_NODISCARD static auto getSystemArch() -> String;

    /**
     * @brief Gets the current username.
     * @return Current username.
     */
    ATOM_NODISCARD static auto getCurrentUser() -> String;

    /**
     * @brief Gets the hostname.
     * @return Hostname.
     */
    ATOM_NODISCARD static auto getHostName() -> String;

    /**
     * @brief Environment variable change notification callback.
     */
    using EnvChangeCallback = std::function<void(
        const String& key, const String& oldValue, const String& newValue)>;

    /**
     * @brief Registers environment variable change notification.
     * @param callback Callback function.
     * @return Notification ID for unregistration.
     */
    static auto registerChangeNotification(EnvChangeCallback callback)
        -> size_t;

    /**
     * @brief Unregisters environment variable change notification.
     * @param id Notification ID.
     * @return True if successfully unregistered, otherwise false.
     */
    static auto unregisterChangeNotification(size_t id) -> bool;

    /**
     * @brief Temporary environment variable scope class.
     */
    class ScopedEnv {
    public:
        /**
         * @brief Constructor, sets temporary environment variable.
         * @param key Environment variable name.
         * @param value Environment variable value.
         */
        ScopedEnv(const String& key, const String& value);

        /**
         * @brief Destructor, restores original environment variable value.
         */
        ~ScopedEnv();

    private:
        String mKey;
        String mOriginalValue;
        bool mHadValue;
    };

    /**
     * @brief Creates a temporary environment variable scope.
     * @param key Environment variable name.
     * @param value Environment variable value.
     * @return Shared pointer to scope object.
     */
    static auto createScopedEnv(const String& key, const String& value)
        -> std::shared_ptr<ScopedEnv>;

#if ATOM_ENABLE_DEBUG
    /**
     * @brief Prints all environment variables.
     */
    static void printAllVariables();

    /**
     * @brief Prints all command-line arguments.
     */
    void printAllArgs() const;
#endif

private:
    class Impl;
    std::shared_ptr<Impl> impl_;

    static HashMap<size_t, EnvChangeCallback> sChangeCallbacks;
    static std::mutex sCallbackMutex;
    static size_t sNextCallbackId;

    static void notifyChangeCallbacks(const String& key, const String& oldValue,
                                      const String& newValue);

    template <typename T>
    static T convertFromString(const String& str, const T& defaultValue);

    static auto splitPathString(const String& pathStr) -> Vector<String>;
    static auto joinPathString(const Vector<String>& paths) -> String;
    static auto getPathSeparator() -> char;
};

template <typename T>
auto Env::getAs(const String& key, const T& default_value) -> T {
    String strValue = get(key, "");
    if (strValue.empty()) {
        return default_value;
    }
    return convertFromString<T>(strValue, default_value);
}

template <typename T>
auto Env::getOptional(const String& key) -> std::optional<T> {
    String strValue = get(key, "");
    if (strValue.empty()) {
        return std::nullopt;
    }
    try {
        return convertFromString<T>(strValue, T{});
    } catch (...) {
        return std::nullopt;
    }
}

template <typename T>
auto Env::getEnvAs(const String& key, const T& default_value) -> T {
    String strValue = getEnv(key, "");
    if (strValue.empty()) {
        return default_value;
    }
    return convertFromString<T>(strValue, default_value);
}

template <typename T>
T Env::convertFromString(const String& str, const T& defaultValue) {
    std::stringstream ss(std::string(str.data(), str.length()));

    T value = defaultValue;
    if constexpr (std::is_same_v<T, bool>) {
        std::string lower_str;
        ss >> lower_str;
        std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                       ::tolower);
        if (lower_str == "true" || lower_str == "1" || lower_str == "yes" ||
            lower_str == "on") {
            value = true;
        } else if (lower_str == "false" || lower_str == "0" ||
                   lower_str == "no" || lower_str == "off") {
            value = false;
        }
    } else if constexpr (std::is_same_v<T, String>) {
        value = str;
    } else {
        if (!(ss >> value) || !ss.eof()) {
            return defaultValue;
        }
    }
    return value;
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_ENV_HPP

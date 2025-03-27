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

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"

namespace atom::utils {
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
     * @return Unordered map of environment variables.
     */
    static auto Environ() -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Adds a key-value pair to the environment variables.
     * @param key The key name.
     * @param val The value associated with the key.
     */
    void add(const std::string& key, const std::string& val);

    /**
     * @brief Adds multiple key-value pairs to the environment variables.
     * @param vars The map of key-value pairs to add.
     */
    void addMultiple(const std::unordered_map<std::string, std::string>& vars);

    /**
     * @brief Checks if a key exists in the environment variables.
     * @param key The key name.
     * @return True if the key exists, otherwise false.
     */
    bool has(const std::string& key);

    /**
     * @brief Checks if all keys exist in the environment variables.
     * @param keys The vector of key names.
     * @return True if all keys exist, otherwise false.
     */
    bool hasAll(const std::vector<std::string>& keys);

    /**
     * @brief Checks if any of the keys exist in the environment variables.
     * @param keys The vector of key names.
     * @return True if any key exists, otherwise false.
     */
    bool hasAny(const std::vector<std::string>& keys);

    /**
     * @brief Deletes a key-value pair from the environment variables.
     * @param key The key name.
     */
    void del(const std::string& key);

    /**
     * @brief Deletes multiple key-value pairs from the environment variables.
     * @param keys The vector of key names to delete.
     */
    void delMultiple(const std::vector<std::string>& keys);

    /**
     * @brief Gets the value associated with a key, or returns a default value
     * if the key does not exist.
     * @param key The key name.
     * @param default_value The default value to return if the key does not
     * exist.
     * @return The value associated with the key, or the default value.
     */
    ATOM_NODISCARD auto get(const std::string& key,
                            const std::string& default_value = "")
        -> std::string;

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
    ATOM_NODISCARD auto getAs(const std::string& key,
                              const T& default_value = T()) -> T;

    /**
     * @brief Gets the value associated with a key as an optional type.
     * @tparam T The type to convert the value to.
     * @param key The key name.
     * @return An optional containing the value if it exists and can be
     * converted, otherwise empty.
     */
    template <typename T>
    ATOM_NODISCARD auto getOptional(const std::string& key) -> std::optional<T>;

    /**
     * @brief Sets the value of an environment variable.
     * @param key The key name.
     * @param val The value to set.
     * @return True if the environment variable was set successfully, otherwise
     * false.
     */
    auto setEnv(const std::string& key, const std::string& val) -> bool;

    /**
     * @brief Sets multiple environment variables.
     * @param vars The map of key-value pairs to set.
     * @return True if all environment variables were set successfully,
     * otherwise false.
     */
    auto setEnvMultiple(
        const std::unordered_map<std::string, std::string>& vars) -> bool;

    /**
     * @brief Gets the value of an environment variable, or returns a default
     * value if the variable does not exist.
     * @param key The key name.
     * @param default_value The default value to return if the variable does not
     * exist.
     * @return The value of the environment variable, or the default value.
     */
    ATOM_NODISCARD auto getEnv(const std::string& key,
                               const std::string& default_value = "")
        -> std::string;

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
    ATOM_NODISCARD auto getEnvAs(const std::string& key,
                                 const T& default_value = T()) -> T;

    /**
     * @brief Unsets an environment variable.
     * @param name The name of the environment variable to unset.
     */
    void unsetEnv(const std::string& name);

    /**
     * @brief Unsets multiple environment variables.
     * @param names The vector of environment variable names to unset.
     */
    void unsetEnvMultiple(const std::vector<std::string>& names);

    /**
     * @brief Lists all environment variables.
     * @return A vector of environment variable names.
     */
    static auto listVariables() -> std::vector<std::string>;

    /**
     * @brief Filters environment variables based on a predicate.
     * @param predicate The predicate function that takes a key-value pair and
     * returns a boolean.
     * @return A map of filtered environment variables.
     */
    static auto filterVariables(
        const std::function<bool(const std::string&, const std::string&)>&
            predicate) -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Gets all environment variables that start with a given prefix.
     * @param prefix The prefix to filter by.
     * @return A map of environment variables with the given prefix.
     */
    static auto getVariablesWithPrefix(const std::string& prefix)
        -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Saves environment variables to a file.
     * @param filePath The path to the file.
     * @param vars The map of variables to save, or all environment variables if
     * empty.
     * @return True if the save was successful, otherwise false.
     */
    static auto saveToFile(
        const std::filesystem::path& filePath,
        const std::unordered_map<std::string, std::string>& vars = {}) -> bool;

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
    ATOM_NODISCARD auto getExecutablePath() const -> std::string;

    /**
     * @brief Gets the working directory.
     * @return The working directory.
     */
    ATOM_NODISCARD auto getWorkingDirectory() const -> std::string;

    /**
     * @brief Gets the program name.
     * @return The program name.
     */
    ATOM_NODISCARD auto getProgramName() const -> std::string;

    /**
     * @brief Gets all command-line arguments.
     * @return The map of command-line arguments.
     */
    ATOM_NODISCARD auto getAllArgs() const
        -> std::unordered_map<std::string, std::string>;

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

    // Helper method for string to numeric conversion
    template <typename T>
    static T convertFromString(const std::string& str, const T& defaultValue);
};

// Template implementation
template <typename T>
auto Env::getAs(const std::string& key, const T& default_value) -> T {
    std::string strValue = get(key, "");
    if (strValue.empty()) {
        return default_value;
    }
    return convertFromString<T>(strValue, default_value);
}

template <typename T>
auto Env::getOptional(const std::string& key) -> std::optional<T> {
    std::string strValue = get(key, "");
    if (strValue.empty()) {
        return std::nullopt;
    }
    try {
        return convertFromString<T>(strValue, T());
    } catch (...) {
        return std::nullopt;
    }
}

template <typename T>
auto Env::getEnvAs(const std::string& key, const T& default_value) -> T {
    std::string strValue = getEnv(key, "");
    if (strValue.empty()) {
        return default_value;
    }
    return convertFromString<T>(strValue, default_value);
}

template <typename T>
T Env::convertFromString(const std::string& str, const T& defaultValue) {
    T value = defaultValue;
    try {
        if constexpr (std::is_same_v<T, int>) {
            value = std::stoi(str);
        } else if constexpr (std::is_same_v<T, long>) {
            value = std::stol(str);
        } else if constexpr (std::is_same_v<T, long long>) {
            value = std::stoll(str);
        } else if constexpr (std::is_same_v<T, unsigned long>) {
            value = std::stoul(str);
        } else if constexpr (std::is_same_v<T, unsigned long long>) {
            value = std::stoull(str);
        } else if constexpr (std::is_same_v<T, float>) {
            value = std::stof(str);
        } else if constexpr (std::is_same_v<T, double>) {
            value = std::stod(str);
        } else if constexpr (std::is_same_v<T, long double>) {
            value = std::stold(str);
        } else if constexpr (std::is_same_v<T, bool>) {
            value =
                (str == "true" || str == "1" || str == "yes" || str == "on");
        } else if constexpr (std::is_same_v<T, std::string>) {
            value = str;
        } else {
            // Unsupported type, return default value
        }
    } catch (...) {
        return defaultValue;
    }
    return value;
}

}  // namespace atom::utils

#endif

/*
 * env.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Environment variable management - Main header file

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

// Include all modular environment components
#include "env/env_core.hpp"
#include "env/env_file_io.hpp"
#include "env/env_path.hpp"
#include "env/env_persistent.hpp"
#include "env/env_scoped.hpp"
#include "env/env_system.hpp"
#include "env/env_utils.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

/**
 * @brief Main Environment variable class that provides a unified interface
 * to all environment management functionality.
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

    // Instance methods for local environment management
    void add(const String& key, const String& val);
    void addMultiple(const HashMap<String, String>& vars);
    bool has(const String& key);
    bool hasAll(const Vector<String>& keys);
    bool hasAny(const Vector<String>& keys);
    void del(const String& key);
    void delMultiple(const Vector<String>& keys);
    ATOM_NODISCARD auto get(const String& key, const String& default_value = "")
        -> String;

    template <typename T>
    ATOM_NODISCARD auto getAs(const String& key, const T& default_value = T())
        -> T;

    template <typename T>
    ATOM_NODISCARD auto getOptional(const String& key) -> std::optional<T>;

    // Static methods for process environment management
    static auto setEnv(const String& key, const String& val) -> bool;
    static auto setEnvMultiple(const HashMap<String, String>& vars) -> bool;
    ATOM_NODISCARD static auto getEnv(const String& key,
                                      const String& default_value = "")
        -> String;

    template <typename T>
    ATOM_NODISCARD static auto getEnvAs(const String& key,
                                        const T& default_value = T()) -> T;

    static void unsetEnv(const String& name);
    static void unsetEnvMultiple(const Vector<String>& names);
    static auto listVariables() -> Vector<String>;
    static auto filterVariables(
        const std::function<bool(const String&, const String&)>& predicate)
        -> HashMap<String, String>;
    static auto getVariablesWithPrefix(const String& prefix)
        -> HashMap<String, String>;

    // File I/O methods
    static auto saveToFile(const std::filesystem::path& filePath,
                           const HashMap<String, String>& vars = {}) -> bool;
    static auto loadFromFile(const std::filesystem::path& filePath,
                             bool overwrite = false) -> bool;

    // Program information methods
    ATOM_NODISCARD auto getExecutablePath() const -> String;
    ATOM_NODISCARD auto getWorkingDirectory() const -> String;
    ATOM_NODISCARD auto getProgramName() const -> String;
    ATOM_NODISCARD auto getAllArgs() const -> HashMap<String, String>;

    // System directory methods
    ATOM_NODISCARD static auto getHomeDir() -> String;
    ATOM_NODISCARD static auto getTempDir() -> String;
    ATOM_NODISCARD static auto getConfigDir() -> String;
    ATOM_NODISCARD static auto getDataDir() -> String;

    // Variable expansion and utilities
    ATOM_NODISCARD static auto expandVariables(
        const String& str, VariableFormat format = VariableFormat::AUTO)
        -> String;

    // Persistent environment methods
    static auto setPersistentEnv(const String& key, const String& val,
                                 PersistLevel level = PersistLevel::USER)
        -> bool;
    static auto deletePersistentEnv(const String& key,
                                    PersistLevel level = PersistLevel::USER)
        -> bool;

    // PATH environment methods
    static auto addToPath(const String& path, bool prepend = false) -> bool;
    static auto removeFromPath(const String& path) -> bool;
    ATOM_NODISCARD static auto isInPath(const String& path) -> bool;
    ATOM_NODISCARD static auto getPathEntries() -> Vector<String>;

    // Environment comparison and merging
    ATOM_NODISCARD static auto diffEnvironments(
        const HashMap<String, String>& env1,
        const HashMap<String, String>& env2)
        -> std::tuple<HashMap<String, String>, HashMap<String, String>,
                      HashMap<String, String>>;
    ATOM_NODISCARD static auto mergeEnvironments(
        const HashMap<String, String>& baseEnv,
        const HashMap<String, String>& overlayEnv, bool override = true)
        -> HashMap<String, String>;

    // System information methods
    ATOM_NODISCARD static auto getSystemName() -> String;
    ATOM_NODISCARD static auto getSystemArch() -> String;
    ATOM_NODISCARD static auto getCurrentUser() -> String;
    ATOM_NODISCARD static auto getHostName() -> String;

    // Notification methods
    using EnvChangeCallback = std::function<void(
        const String& key, const String& oldValue, const String& newValue)>;
    static auto registerChangeNotification(EnvChangeCallback callback)
        -> size_t;
    static auto unregisterChangeNotification(size_t id) -> bool;

    // Scoped environment variable class
    class ScopedEnv {
    public:
        ScopedEnv(const String& key, const String& value);
        ~ScopedEnv();

    private:
        String mKey;
        String mOriginalValue;
        bool mHadValue;
    };
    static auto createScopedEnv(const String& key, const String& value)
        -> std::shared_ptr<ScopedEnv>;

#if ATOM_ENABLE_DEBUG
    static void printAllVariables();
    void printAllArgs() const;
#endif

private:
    class Impl;
    std::shared_ptr<Impl> impl_;

    template <typename T>
    static T convertFromString(const String& str, const T& defaultValue);
};

// Template method implementations
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

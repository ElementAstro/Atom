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

#include <cstdlib>  // For getenv, setenv, unsetenv
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>  // For string conversion in convertFromString
#include <tuple>    // For std::tuple return type
#include <type_traits>

#include "atom/containers/high_performance.hpp"  // Include high performance containers
#include "atom/macro.hpp"

namespace atom::utils {

// Use type aliases from high_performance.hpp
using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

/**
 * @brief 环境变量格式枚举
 */
enum class VariableFormat {
    UNIX,     // ${VAR} or $VAR format
    WINDOWS,  // %VAR% format
    AUTO      // Auto-detect based on platform
};

/**
 * @brief 环境变量持久化级别枚举
 */
enum class PersistLevel {
    PROCESS,  // 仅当前进程有效
    USER,     // 用户级别持久化
    SYSTEM    // 系统级别持久化（需要管理员权限）
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
     * @brief 获取用户主目录
     * @return 返回用户主目录的路径
     */
    ATOM_NODISCARD static auto getHomeDir() -> String;

    /**
     * @brief 获取系统临时目录
     * @return 返回系统临时目录的路径
     */
    ATOM_NODISCARD static auto getTempDir() -> String;

    /**
     * @brief 获取系统配置目录
     * @return 返回系统配置目录的路径
     */
    ATOM_NODISCARD static auto getConfigDir() -> String;

    /**
     * @brief 获取用户数据目录
     * @return 返回用户数据目录的路径
     */
    ATOM_NODISCARD static auto getDataDir() -> String;

    /**
     * @brief 扩展字符串中的环境变量引用
     * @param str 包含环境变量引用的字符串（如 "$HOME/file" 或
     * "%PATH%;newpath"）
     * @param format 环境变量格式，可以是 Unix 风格 (${VAR}) 或 Windows 风格
     * (%VAR%)
     * @return 扩展后的字符串
     */
    ATOM_NODISCARD static auto expandVariables(
        const String& str, VariableFormat format = VariableFormat::AUTO)
        -> String;

    /**
     * @brief 持久化设置环境变量
     * @param key 环境变量名称
     * @param val 环境变量值
     * @param level 持久化级别
     * @return 是否成功持久化
     */
    static auto setPersistentEnv(const String& key, const String& val,
                                 PersistLevel level = PersistLevel::USER)
        -> bool;

    /**
     * @brief 持久化删除环境变量
     * @param key 环境变量名称
     * @param level 持久化级别
     * @return 是否成功删除
     */
    static auto deletePersistentEnv(const String& key,
                                    PersistLevel level = PersistLevel::USER)
        -> bool;

    /**
     * @brief 向 PATH 环境变量添加路径
     * @param path 要添加的路径
     * @param prepend 是否添加到开头（默认添加到末尾）
     * @return 是否成功添加
     */
    static auto addToPath(const String& path, bool prepend = false) -> bool;

    /**
     * @brief 从 PATH 环境变量中移除路径
     * @param path 要移除的路径
     * @return 是否成功移除
     */
    static auto removeFromPath(const String& path) -> bool;

    /**
     * @brief 检查路径是否在 PATH 环境变量中
     * @param path 要检查的路径
     * @return 是否在 PATH 中
     */
    ATOM_NODISCARD static auto isInPath(const String& path) -> bool;

    /**
     * @brief 获取 PATH 环境变量中的所有路径
     * @return 包含所有路径的向量
     */
    ATOM_NODISCARD static auto getPathEntries() -> Vector<String>;

    /**
     * @brief 比较两个环境变量集合的差异
     * @param env1 第一个环境变量集合
     * @param env2 第二个环境变量集合
     * @return 差异内容，包括新增、删除和修改的变量
     */
    ATOM_NODISCARD static auto diffEnvironments(
        const HashMap<String, String>& env1,
        const HashMap<String, String>& env2)
        -> std::tuple<HashMap<String, String>,   // 新增的变量
                      HashMap<String, String>,   // 删除的变量
                      HashMap<String, String>>;  // 修改的变量

    /**
     * @brief 合并两个环境变量集合
     * @param baseEnv 基础环境变量集合
     * @param overlayEnv 覆盖的环境变量集合
     * @param override 冲突时是否覆盖基础环境变量
     * @return 合并后的环境变量集合
     */
    ATOM_NODISCARD static auto mergeEnvironments(
        const HashMap<String, String>& baseEnv,
        const HashMap<String, String>& overlayEnv, bool override = true)
        -> HashMap<String, String>;

    /**
     * @brief 获取系统名称
     * @return 系统名称（如 "Windows"、"Linux"、"MacOS"）
     */
    ATOM_NODISCARD static auto getSystemName() -> String;

    /**
     * @brief 获取系统架构
     * @return 系统架构（如 "x86_64"、"arm64"）
     */
    ATOM_NODISCARD static auto getSystemArch() -> String;

    /**
     * @brief 获取当前用户名
     * @return 当前用户名
     */
    ATOM_NODISCARD static auto getCurrentUser() -> String;

    /**
     * @brief 获取主机名
     * @return 主机名
     */
    ATOM_NODISCARD static auto getHostName() -> String;

    /**
     * @brief 环境变量更改通知回调
     */
    using EnvChangeCallback = std::function<void(
        const String& key, const String& oldValue, const String& newValue)>;

    /**
     * @brief 注册环境变量更改通知
     * @param callback 回调函数
     * @return 通知ID，用于注销
     */
    static auto registerChangeNotification(EnvChangeCallback callback)
        -> size_t;

    /**
     * @brief 注销环境变量更改通知
     * @param id 通知ID
     * @return 是否成功注销
     */
    static auto unregisterChangeNotification(size_t id) -> bool;

    /**
     * @brief 临时环境变量作用域类
     */
    class ScopedEnv {
    public:
        /**
         * @brief 构造函数，设置临时环境变量
         * @param key 环境变量名称
         * @param value 环境变量值
         */
        ScopedEnv(const String& key, const String& value);

        /**
         * @brief 析构函数，恢复环境变量原值
         */
        ~ScopedEnv();

    private:
        String mKey;
        String mOriginalValue;
        bool mHadValue;
    };

    /**
     * @brief 创建一个临时环境变量作用域
     * @param key 环境变量名称
     * @param value 环境变量值
     * @return 作用域对象的共享指针
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

    // 存储环境变量更改通知回调的静态成员
    static HashMap<size_t, EnvChangeCallback> sChangeCallbacks;
    static std::mutex sCallbackMutex;
    static size_t sNextCallbackId;

    // 通知所有注册的回调函数
    static void notifyChangeCallbacks(const String& key, const String& oldValue,
                                      const String& newValue);

    // Helper method for string to numeric conversion
    template <typename T>
    static T convertFromString(const String& str, const T& defaultValue);

    // Helper method for splitting PATH-like strings
    static auto splitPathString(const String& pathStr) -> Vector<String>;

    // Helper method for joining PATH-like strings
    static auto joinPathString(const Vector<String>& paths) -> String;

    // Helper method to get platform-specific path separator
    static auto getPathSeparator() -> char;
};

// Template implementation
template <typename T>
auto Env::getAs(const String& key, const T& default_value) -> T {
    String strValue = get(key, "");
    // Assuming String has empty() method
    if (strValue.empty()) {
        return default_value;
    }
    return convertFromString<T>(strValue, default_value);
}

template <typename T>
auto Env::getOptional(const String& key) -> std::optional<T> {
    String strValue = get(key, "");
    // Assuming String has empty() method
    if (strValue.empty()) {
        return std::nullopt;
    }
    try {
        // Assuming convertFromString throws on failure for relevant types
        return convertFromString<T>(strValue, T{});
    } catch (...) {
        return std::nullopt;
    }
}

template <typename T>
auto Env::getEnvAs(const String& key, const T& default_value) -> T {
    String strValue = getEnv(key, "");
    // Assuming String has empty() method
    if (strValue.empty()) {
        return default_value;
    }
    return convertFromString<T>(strValue, default_value);
}

template <typename T>
T Env::convertFromString(const String& str, const T& defaultValue) {
    // Assuming String can be implicitly converted to std::string
    // or provides a compatible stream insertion operator.
    // If not, use str.c_str() or str.data() for std::string construction
    // or direct parsing.
    // Using std::stringstream for conversion as a general approach.
    // Requires String to be streamable or convertible to std::string.

    // Option 1: Assuming String is streamable
    // std::stringstream ss;
    // ss << str;

    // Option 2: Assuming String has c_str() or data()
    std::stringstream ss(
        std::string(str.data(), str.length()));  // Construct std::string

    T value = defaultValue;
    if constexpr (std::is_same_v<T, bool>) {
        // Handle bool separately for more flexible parsing
        std::string lower_str;
        ss >> lower_str;
        // Convert to lower case for case-insensitive comparison
        std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                       ::tolower);
        if (lower_str == "true" || lower_str == "1" || lower_str == "yes" ||
            lower_str == "on") {
            value = true;
        } else if (lower_str == "false" || lower_str == "0" ||
                   lower_str == "no" || lower_str == "off") {
            value = false;
        } else {
            // Conversion failed, keep default
        }
    } else if constexpr (std::is_same_v<T, String>) {
        // Already a String, just return it (or handle potential stream
        // extraction issues if needed) If using stringstream, extract back into
        // a String if necessary For simplicity, assume direct assignment is
        // okay here.
        value = str;
    } else {
        // Attempt to extract numeric types
        if (!(ss >> value) || !ss.eof()) {
            // Conversion failed or extra characters found, return default
            return defaultValue;
        }
    }
    return value;
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_ENV_HPP

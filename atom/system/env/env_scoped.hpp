/*
 * env_scoped.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Scoped environment variable management

**************************************************/

#ifndef ATOM_SYSTEM_ENV_SCOPED_HPP
#define ATOM_SYSTEM_ENV_SCOPED_HPP

#include <memory>
#include <vector>
#include <functional>
#include <chrono>

#include "atom/containers/high_performance.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

/**
 * @brief Scoped environment variable restoration callback
 */
using ScopeRestoreCallback = std::function<void(const String& key, const String& oldValue, const String& newValue)>;

/**
 * @brief Temporary environment variable scope class
 */
class ScopedEnv {
public:
    /**
     * @brief Constructor, sets temporary environment variable
     * @param key Environment variable name
     * @param value Environment variable value
     */
    ScopedEnv(const String& key, const String& value);

    /**
     * @brief Constructor with restore callback
     * @param key Environment variable name
     * @param value Environment variable value
     * @param callback Callback to execute on restore
     */
    ScopedEnv(const String& key, const String& value, ScopeRestoreCallback callback);

    /**
     * @brief Destructor, restores original environment variable value
     */
    ~ScopedEnv();

    // Non-copyable but movable
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;
    ScopedEnv(ScopedEnv&&) = default;
    ScopedEnv& operator=(ScopedEnv&&) = default;

    /**
     * @brief Gets the key being managed
     * @return Environment variable key
     */
    const String& getKey() const { return mKey; }

    /**
     * @brief Gets the current value
     * @return Current environment variable value
     */
    String getCurrentValue() const;

    /**
     * @brief Gets the original value
     * @return Original environment variable value
     */
    const String& getOriginalValue() const { return mOriginalValue; }

    /**
     * @brief Checks if the variable had a value before scoping
     * @return True if variable existed before scoping
     */
    bool hadOriginalValue() const { return mHadValue; }

    /**
     * @brief Updates the scoped value
     * @param newValue New value to set
     */
    void updateValue(const String& newValue);

    /**
     * @brief Manually restores the original value (scope becomes inactive)
     */
    void restore();

    /**
     * @brief Checks if the scope is still active
     * @return True if scope is active
     */
    bool isActive() const { return mActive; }

    /**
     * @brief Gets the creation timestamp
     * @return Time when scope was created
     */
    std::chrono::steady_clock::time_point getCreationTime() const { return mCreationTime; }

private:
    String mKey;
    String mOriginalValue;
    bool mHadValue;
    bool mActive;
    std::chrono::steady_clock::time_point mCreationTime;
    ScopeRestoreCallback mRestoreCallback;
};

/**
 * @brief Batch scoped environment variable manager
 */
class ScopedEnvBatch {
public:
    /**
     * @brief Constructor with environment variables map
     * @param vars Map of key-value pairs to set
     */
    explicit ScopedEnvBatch(const HashMap<String, String>& vars);

    /**
     * @brief Constructor with vector of key-value pairs
     * @param vars Vector of key-value pairs to set
     */
    explicit ScopedEnvBatch(const Vector<std::pair<String, String>>& vars);

    /**
     * @brief Destructor, restores all original values
     */
    ~ScopedEnvBatch();

    // Non-copyable but movable
    ScopedEnvBatch(const ScopedEnvBatch&) = delete;
    ScopedEnvBatch& operator=(const ScopedEnvBatch&) = delete;
    ScopedEnvBatch(ScopedEnvBatch&&) = default;
    ScopedEnvBatch& operator=(ScopedEnvBatch&&) = default;

    /**
     * @brief Adds a new scoped variable to the batch
     * @param key Environment variable name
     * @param value Environment variable value
     */
    void addVariable(const String& key, const String& value);

    /**
     * @brief Updates an existing scoped variable
     * @param key Environment variable name
     * @param newValue New value to set
     * @return True if variable was found and updated
     */
    bool updateVariable(const String& key, const String& newValue);

    /**
     * @brief Removes a variable from the batch (restores original value)
     * @param key Environment variable name
     * @return True if variable was found and removed
     */
    bool removeVariable(const String& key);

    /**
     * @brief Gets the number of variables in the batch
     * @return Number of scoped variables
     */
    size_t size() const { return mScopes.size(); }

    /**
     * @brief Checks if the batch is empty
     * @return True if no variables are scoped
     */
    bool empty() const { return mScopes.empty(); }

    /**
     * @brief Gets all scoped variable keys
     * @return Vector of keys
     */
    Vector<String> getKeys() const;

    /**
     * @brief Manually restores all variables (batch becomes inactive)
     */
    void restoreAll();

private:
    Vector<std::unique_ptr<ScopedEnv>> mScopes;
    bool mActive;
};

/**
 * @brief Scoped environment variable management utilities
 */
class EnvScoped {
public:
    /**
     * @brief Creates a temporary environment variable scope
     * @param key Environment variable name
     * @param value Environment variable value
     * @return Shared pointer to scope object
     */
    static auto createScopedEnv(const String& key, const String& value)
        -> std::shared_ptr<ScopedEnv>;

    /**
     * @brief Creates a unique scoped environment variable
     * @param key Environment variable name
     * @param value Environment variable value
     * @return Unique pointer to scope object
     */
    static auto createUniqueScopedEnv(const String& key, const String& value)
        -> std::unique_ptr<ScopedEnv>;

    /**
     * @brief Creates a scoped environment variable with callback
     * @param key Environment variable name
     * @param value Environment variable value
     * @param callback Callback to execute on restore
     * @return Unique pointer to scope object
     */
    static auto createScopedEnvWithCallback(const String& key, const String& value,
                                            ScopeRestoreCallback callback)
        -> std::unique_ptr<ScopedEnv>;

    /**
     * @brief Creates a batch scoped environment manager
     * @param vars Map of key-value pairs to set
     * @return Unique pointer to batch scope object
     */
    static auto createScopedEnvBatch(const HashMap<String, String>& vars)
        -> std::unique_ptr<ScopedEnvBatch>;

    /**
     * @brief Creates a temporary environment scope for a code block
     * @param vars Map of key-value pairs to set
     * @param func Function to execute with scoped environment
     * @return Result of the function execution
     */
    template<typename Func>
    static auto withScopedEnv(const HashMap<String, String>& vars, Func&& func) -> decltype(func());

    /**
     * @brief Creates a nested environment scope
     * @param parentScope Parent scope to inherit from
     * @param additionalVars Additional variables to set
     * @return Unique pointer to nested scope object
     */
    static auto createNestedScope(const ScopedEnv& parentScope,
                                  const HashMap<String, String>& additionalVars)
        -> std::unique_ptr<ScopedEnvBatch>;

    /**
     * @brief Gets statistics about active scopes
     * @return Map containing scope statistics
     */
    static auto getScopeStatistics() -> HashMap<String, size_t>;

    /**
     * @brief Lists all currently active scoped variables
     * @return Vector of active scope information
     */
    static auto listActiveScopes() -> Vector<String>;

private:
    static Vector<ScopedEnv*> sActiveScopes;
    static std::mutex sScopesMutex;

    static void registerScope(ScopedEnv* scope);
    static void unregisterScope(ScopedEnv* scope);

    friend class ScopedEnv;
};

// Template implementation
template<typename Func>
auto EnvScoped::withScopedEnv(const HashMap<String, String>& vars, Func&& func) -> decltype(func()) {
    auto batch = createScopedEnvBatch(vars);
    return func();
}

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_SCOPED_HPP

/*
 * env_persistent.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Persistent environment variable management

**************************************************/

#ifndef ATOM_SYSTEM_ENV_PERSISTENT_HPP
#define ATOM_SYSTEM_ENV_PERSISTENT_HPP

#include <mutex>

#ifdef _WIN32
#include <windows.h>  // HKEY and registry APIs used in the _WIN32 declarations below
#endif

#include "atom/containers/high_performance.hpp"
#include "env_core.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

/**
 * @brief Persistence operation result enumeration
 */
enum class PersistenceResult {
    SUCCESS,
    ALREADY_EXISTS,
    NOT_FOUND,
    PERMISSION_DENIED,
    INVALID_KEY,
    INVALID_VALUE,
    SYSTEM_ERROR,
    BACKUP_FAILED
};

/**
 * @brief Persistent environment variable management
 */
class EnvPersistent {
public:
    /**
     * @brief Sets a persistent environment variable
     * @param key Environment variable name
     * @param val Environment variable value
     * @param level Persistence level
     * @return Result of the operation
     */
    static auto setPersistentEnv(const String& key, const String& val,
                                 PersistLevel level = PersistLevel::USER)
        -> PersistenceResult;

    /**
     * @brief Sets multiple persistent environment variables
     * @param vars Map of key-value pairs to set
     * @param level Persistence level
     * @return Number of successfully set variables
     */
    static auto setPersistentEnvBatch(const HashMap<String, String>& vars,
                                      PersistLevel level = PersistLevel::USER)
        -> size_t;

    /**
     * @brief Deletes a persistent environment variable
     * @param key Environment variable name
     * @param level Persistence level
     * @return Result of the operation
     */
    static auto deletePersistentEnv(const String& key,
                                    PersistLevel level = PersistLevel::USER)
        -> PersistenceResult;

    /**
     * @brief Deletes multiple persistent environment variables
     * @param keys Vector of keys to delete
     * @param level Persistence level
     * @return Number of successfully deleted variables
     */
    static auto deletePersistentEnvBatch(const Vector<String>& keys,
                                         PersistLevel level = PersistLevel::USER)
        -> size_t;

    /**
     * @brief Gets a persistent environment variable
     * @param key Environment variable name
     * @param level Persistence level
     * @return Value if found, empty string otherwise
     */
    static auto getPersistentEnv(const String& key,
                                 PersistLevel level = PersistLevel::USER)
        -> String;

    /**
     * @brief Lists all persistent environment variables
     * @param level Persistence level
     * @return Map of persistent environment variables
     */
    static auto listPersistentEnv(PersistLevel level = PersistLevel::USER)
        -> HashMap<String, String>;

    /**
     * @brief Validates a persistent environment variable
     * @param key Environment variable name
     * @param value Environment variable value
     * @param level Persistence level
     * @return True if valid for persistence, false otherwise
     */
    static auto validatePersistentEnv(const String& key, const String& value,
                                      PersistLevel level = PersistLevel::USER)
        -> bool;

    /**
     * @brief Backs up current persistent environment variables
     * @param name Backup name identifier
     * @param level Persistence level
     * @return True if backup was successful
     */
    static auto backupPersistentEnv(const String& name,
                                    PersistLevel level = PersistLevel::USER)
        -> bool;

    /**
     * @brief Restores persistent environment variables from backup
     * @param name Backup name identifier
     * @param level Persistence level
     * @return True if restore was successful
     */
    static auto restorePersistentEnv(const String& name,
                                     PersistLevel level = PersistLevel::USER)
        -> bool;

    /**
     * @brief Lists available persistent environment backups
     * @param level Persistence level
     * @return Vector of backup names
     */
    static auto listPersistentBackups(PersistLevel level = PersistLevel::USER)
        -> Vector<String>;

private:
#ifdef _WIN32
    /**
     * @brief Sets a persistent environment variable on Windows
     * @param key Environment variable name
     * @param val Environment variable value
     * @param level Persistence level
     * @return True if successfully set, otherwise false
     */
    static auto setPersistentEnvWindows(const String& key, const String& val,
                                        PersistLevel level) -> bool;

    /**
     * @brief Deletes a persistent environment variable on Windows
     * @param key Environment variable name
     * @param level Persistence level
     * @return True if successfully deleted, otherwise false
     */
    static auto deletePersistentEnvWindows(const String& key,
                                           PersistLevel level) -> bool;
#else
    /**
     * @brief Sets a persistent environment variable on Unix-like systems
     * @param key Environment variable name
     * @param val Environment variable value
     * @param level Persistence level
     * @return True if successfully set, otherwise false
     */
    static auto setPersistentEnvUnix(const String& key, const String& val,
                                     PersistLevel level) -> bool;

    /**
     * @brief Deletes a persistent environment variable on Unix-like systems
     * @param key Environment variable name
     * @param level Persistence level
     * @return True if successfully deleted, otherwise false
     */
    static auto deletePersistentEnvUnix(const String& key,
                                        PersistLevel level) -> bool;

    /**
     * @brief Gets the appropriate shell profile file path
     * @param homeDir The user's home directory
     * @return Path to the shell profile file
     */
    static auto getShellProfilePath(const String& homeDir) -> String;
#endif

    // Backup system
    static HashMap<String, HashMap<String, String>> sPersistentBackups;
    static std::mutex sBackupMutex;

    // Helper methods
    static auto validateKey(const String& key) -> bool;
    static auto validateValue(const String& value) -> bool;
    static auto createBackupPath(const String& name, PersistLevel level) -> String;
    static auto getBackupDirectory() -> String;
    static auto ensureBackupDirectory() -> bool;

#ifdef _WIN32
    static auto getRegistryPath(PersistLevel level) -> std::pair<HKEY, String>;
    static auto readRegistryValue(HKEY rootKey, const String& subKey, const String& valueName) -> String;
    static auto writeRegistryValue(HKEY rootKey, const String& subKey, const String& valueName, const String& value) -> bool;
    static auto deleteRegistryValue(HKEY rootKey, const String& subKey, const String& valueName) -> bool;
    static auto listRegistryValues(HKEY rootKey, const String& subKey) -> HashMap<String, String>;
#else
    static auto readShellProfile(const String& filePath) -> HashMap<String, String>;
    static auto writeShellProfile(const String& filePath, const HashMap<String, String>& vars) -> bool;
    static auto appendToShellProfile(const String& filePath, const String& key, const String& value) -> bool;
    static auto removeFromShellProfile(const String& filePath, const String& key) -> bool;
#endif
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_PERSISTENT_HPP

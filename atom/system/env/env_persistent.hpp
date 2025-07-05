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

#include "atom/containers/high_performance.hpp"
#include "env_core.hpp"

namespace atom::utils {

using atom::containers::String;

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
     * @return True if successfully persisted, otherwise false
     */
    static auto setPersistentEnv(const String& key, const String& val,
                                 PersistLevel level = PersistLevel::USER)
        -> bool;

    /**
     * @brief Deletes a persistent environment variable
     * @param key Environment variable name
     * @param level Persistence level
     * @return True if successfully deleted, otherwise false
     */
    static auto deletePersistentEnv(const String& key,
                                    PersistLevel level = PersistLevel::USER)
        -> bool;

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
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_PERSISTENT_HPP

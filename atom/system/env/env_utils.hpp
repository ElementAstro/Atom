/*
 * env_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Environment variable utility functions

**************************************************/

#ifndef ATOM_SYSTEM_ENV_UTILS_HPP
#define ATOM_SYSTEM_ENV_UTILS_HPP

#include <tuple>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"
#include "env_core.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;

/**
 * @brief Environment variable utility functions
 */
class EnvUtils {
public:
    /**
     * @brief Expands environment variable references in a string
     * @param str String containing environment variable references (e.g.,
     * "$HOME/file" or "%PATH%;newpath")
     * @param format Environment variable format, can be Unix style (${VAR}) or
     * Windows style (%VAR%)
     * @return Expanded string
     */
    ATOM_NODISCARD static auto expandVariables(
        const String& str, VariableFormat format = VariableFormat::AUTO)
        -> String;

    /**
     * @brief Compares differences between two environment variable sets
     * @param env1 First environment variable set
     * @param env2 Second environment variable set
     * @return Difference content, including added, removed, and modified
     * variables
     */
    ATOM_NODISCARD static auto diffEnvironments(
        const HashMap<String, String>& env1,
        const HashMap<String, String>& env2)
        -> std::tuple<HashMap<String, String>,   // Added variables
                      HashMap<String, String>,   // Removed variables
                      HashMap<String, String>>;  // Modified variables

    /**
     * @brief Merges two environment variable sets
     * @param baseEnv Base environment variable set
     * @param overlayEnv Overlay environment variable set
     * @param override Whether to override base environment variables in case of
     * conflict
     * @return Merged environment variable set
     */
    ATOM_NODISCARD static auto mergeEnvironments(
        const HashMap<String, String>& baseEnv,
        const HashMap<String, String>& overlayEnv, bool override = true)
        -> HashMap<String, String>;

private:
    /**
     * @brief Expands Unix-style variable references (${VAR} or $VAR)
     * @param str The string to expand
     * @return Expanded string
     */
    static auto expandUnixVariables(const String& str) -> String;

    /**
     * @brief Expands Windows-style variable references (%VAR%)
     * @param str The string to expand
     * @return Expanded string
     */
    static auto expandWindowsVariables(const String& str) -> String;

    /**
     * @brief Finds the next variable reference in a string
     * @param str The string to search
     * @param start Starting position
     * @param format Variable format to look for
     * @return Tuple of (found, start_pos, end_pos, var_name)
     */
    static auto findNextVariable(const String& str, size_t start,
                                 VariableFormat format)
        -> std::tuple<bool, size_t, size_t, String>;

    /**
     * @brief Validates a variable name
     * @param name The variable name to validate
     * @return True if valid, otherwise false
     */
    static auto isValidVariableName(const String& name) -> bool;
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_UTILS_HPP
